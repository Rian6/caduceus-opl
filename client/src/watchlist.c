#include "watchlist.h"

#include <string.h>

#include "log.h"
#include "protocol.h"

/* Internal rcheevos headers: the memref pool of the loaded game is the
   only place that knows which addresses the achievement set reads, and
   rcheevos has no public accessor for it. */
#include "rc_client_internal.h"
#include "rc_internal.h"
#include "rc_runtime_types.h"

static unsigned int g_watch[RA_WATCH_MAX];
static unsigned int g_offset[RA_WATCH_MAX]; /* byte offset of each value in a snapshot */
static int g_count = 0;
static int g_bytes = 0;

static unsigned char g_values[RA_SNAP_MAX_BYTES];
static int g_have_values = 0;
static int g_have_nodes = 0;

/* The memref behind each entry, so a pointer chain can name its parent
   by index instead of matching addresses a second time. */
static const rc_memref_t *g_direct_of[RA_WATCH_MAX];

/* Pointer chains for the console, parents always before their children.
   g_node_of[i] is the memref node i serves. */
static struct ra_node g_nodes[RA_NODE_MAX];
static const rc_memref_t *g_node_of[RA_NODE_MAX];
static int g_node_count = 0;
static int g_node_full = 0;

/* Snapshot values are little-endian on the wire, whatever this PC is. */
static unsigned int load_le32(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

/* Bytes the console must read for a memref of this size. Matches what
   the console derives from the packed list entry. */
static int memsize_bytes(uint8_t size)
{
    switch (size) {
        case RC_MEMSIZE_8_BITS:
        case RC_MEMSIZE_BIT_0:
        case RC_MEMSIZE_BIT_1:
        case RC_MEMSIZE_BIT_2:
        case RC_MEMSIZE_BIT_3:
        case RC_MEMSIZE_BIT_4:
        case RC_MEMSIZE_BIT_5:
        case RC_MEMSIZE_BIT_6:
        case RC_MEMSIZE_BIT_7:
        case RC_MEMSIZE_LOW:
        case RC_MEMSIZE_HIGH:
        case RC_MEMSIZE_BITCOUNT:
            return 1;
        case RC_MEMSIZE_16_BITS:
        case RC_MEMSIZE_16_BITS_BE:
            return 2;
        default:
            return 4;
    }
}

/* Chains the console resolves are not counted here. What is left are
   the ones that cannot be compiled into a node: a chain whose offset is
   not a constant, one that hangs off a delta or prior value the console
   keeps no history for, or one over the node or snapshot ceiling. Such
   achievements are NOT disabled. The rcheevos maintainers' guidance (RA
   forum, 28.08.2026): the runtime follows pointers whatever they hold
   and expects a failed read to return 0; since nearly all logic watches
   values change, a permanent 0 does not trigger anything, it just never
   fires. Disabling them would only hide that from the user. So they
   stay active, and their number is reported as "unsupported". */
static int g_indirect = 0;

/* Not every modified memref is a pointer chain. rcheevos also builds
   them for arithmetic between direct reads -- AddSource/SubSource
   chains, combining conditions, prev(x)+prev(y) -- and those the
   console serves fine, since every underlying read is a plain address
   already on the watch list. Only RC_OPERATOR_INDIRECT_READ, anywhere
   up the chain, needs a dereference the console cannot do.

   Transformers: The Game showed the difference: 5 of 76 achievements
   use pointers, all 75 were being disabled. */
static int node_index(const rc_memref_t *m)
{
    int i;

    for (i = 0; i < g_node_count; i++) {
        if (g_node_of[i] == m)
            return i;
    }

    return -1;
}

static int direct_index(const rc_memref_t *m)
{
    int i;

    for (i = 0; i < g_count; i++) {
        if (g_direct_of[i] == m)
            return i;
    }

    return -1;
}

/* Compiles one pointer chain into a node, its parent first, and returns
   the node index. -1 means the console cannot be asked to follow it.

   Recursion ends because a chain is built from the outside in: a
   parent is always an older memref than the child, so no cycle exists.

   What is turned down, and why: a non-constant offset (the console
   computes address = parent + offset and nothing else); a parent read
   as delta or prior (the console keeps no history of past frames); a
   parent that is arithmetic rather than a pointer read. */
static int compile_node(const rc_memref_t *m)
{
    const rc_modified_memref_t *mm;
    const rc_memref_t *pm;
    int parent, from_node, size, at;

    if (m == NULL || m->value.memref_type != RC_MEMREF_TYPE_MODIFIED_MEMREF)
        return -1;

    at = node_index(m);
    if (at >= 0)
        return at;

    mm = (const rc_modified_memref_t *)m;
    if (mm->modifier_type != RC_OPERATOR_INDIRECT_READ)
        return -1;
    if (mm->modifier.type != RC_OPERAND_CONST)
        return -1;
    if (!rc_operand_is_memref(&mm->parent) || mm->parent.type != RC_OPERAND_ADDRESS)
        return -1;

    pm = mm->parent.value.memref;
    if (pm != NULL && pm->value.memref_type == RC_MEMREF_TYPE_MODIFIED_MEMREF) {
        parent = compile_node(pm);
        from_node = 1;
    } else {
        parent = direct_index(pm);
        from_node = 0;
    }

    if (parent < 0 || parent > 0x0FFF)
        return -1;

    if (g_node_count >= RA_NODE_MAX) {
        /* Once, not once per chain: a big set has hundreds. */
        if (!g_node_full) {
            g_node_full = 1;
            log_warn("more than %d pointer chains in this set; the rest stay unsupported", RA_NODE_MAX);
        }
        return -1;
    }

    size = memsize_bytes(rc_memref_shared_size(mm->memref.value.size));
    at = g_node_count++;
    g_nodes[at].w = RA_NODE_PACK((unsigned int)parent, (unsigned int)from_node, (unsigned int)size);
    g_nodes[at].offset = mm->modifier.value.num;
    g_node_of[at] = m;
    return at;
}

/* Every pointer chain in the set, in dependency order. */
static void build_nodes(rc_client_t *client)
{
    rc_memrefs_t *pool = client->game->runtime.memrefs;
    rc_modified_memref_list_t *ml;

    g_node_count = 0;
    g_node_full = 0;

    for (ml = &pool->modified_memrefs; ml != NULL; ml = ml->next) {
        uint16_t k;

        for (k = 0; k < ml->count; k++)
            compile_node(&ml->items[k].memref);
    }

    /* Each node costs eight bytes in every snapshot. Over the ceiling
       the whole set would be refused, which would cost the game its
       telemetry as well, so the chains go and the direct reads stay. */
    if (g_bytes + g_node_count * RA_NODE_PAIR_BYTES > RA_SNAP_MAX_BYTES) {
        log_warn("%d pointer chains do not fit in a snapshot with %d bytes of direct reads; dropping them",
                 g_node_count, g_bytes);
        g_node_count = 0;
    }

    if (g_node_count > 0)
        log_info("%d pointer chain%s the console resolves every frame",
                 g_node_count, g_node_count == 1 ? "" : "s");
}

/* An achievement is unsupported when it reads through a chain that did
   not compile. A chain that did is served like any other address. */
static int memref_unsupported(const rc_memref_t *m)
{
    const rc_modified_memref_t *mm;

    if (m == NULL || m->value.memref_type != RC_MEMREF_TYPE_MODIFIED_MEMREF)
        return 0;

    mm = (const rc_modified_memref_t *)m;
    if (mm->modifier_type == RC_OPERATOR_INDIRECT_READ)
        return node_index(m) < 0;

    if (rc_operand_is_memref(&mm->parent) && memref_unsupported(mm->parent.value.memref))
        return 1;
    if (rc_operand_is_memref(&mm->modifier) && memref_unsupported(mm->modifier.value.memref))
        return 1;

    return 0;
}

static void count_indirect(rc_client_t *client)
{
    rc_client_game_info_t *game = client->game;
    rc_client_subset_info_t *subset;
    int chained = 0;

    g_indirect = 0;

    for (subset = game->subsets; subset != NULL; subset = subset->next) {
        rc_client_achievement_info_t *a = subset->achievements;
        rc_client_achievement_info_t *a_end = a + subset->public_.num_achievements;

        for (; a < a_end; a++) {
            rc_modified_memref_list_t *ml;
            int bad = 0, good = 0;

            if (a->trigger == NULL)
                continue;

            for (ml = &game->runtime.memrefs->modified_memrefs; ml != NULL; ml = ml->next) {
                uint16_t k;

                for (k = 0; k < ml->count; k++) {
                    rc_memref_t *memref = &ml->items[k].memref;

                    if (!rc_trigger_contains_memref(a->trigger, memref))
                        continue;

                    if (memref_unsupported(memref))
                        bad = 1;
                    else if (node_index(memref) >= 0)
                        good = 1;
                }
            }

            if (bad)
                log_trace("achievement %u \"%s\" reads through a chain the console cannot follow; it cannot unlock here",
                          a->public_.id, a->public_.title);
            else if (good)
                log_trace("achievement %u \"%s\" reads through a pointer the console follows",
                          a->public_.id, a->public_.title);

            g_indirect += bad;
            chained += (!bad && good);
        }
    }

    if (chained > 0) {
        log_info("%d achievement%s read through pointers the console now follows",
                 chained, chained == 1 ? "" : "s");
    }

    if (g_indirect > 0) {
        log_info("%d achievement%s read through chains that do not compile; they stay active but will not unlock",
                 g_indirect, g_indirect == 1 ? "" : "s");
    }
}

int watchlist_indirect_count(void)
{
    return g_indirect;
}

/* The order of entries here is the order of values in every snapshot;
   the console reads addresses in the order it received them. Pointer
   chains follow as nodes, and their values as (address, value) pairs
   after the direct ones. */
int watchlist_build(rc_client_t *client)
{
    rc_memrefs_t *pool;
    rc_memref_list_t *ml;
    int off = 0, n = 0;

    g_count = 0;
    g_bytes = 0;
    g_have_values = 0;
    g_have_nodes = 0;
    g_node_count = 0;

    if (client == NULL || client->game == NULL)
        return 0;

    pool = client->game->runtime.memrefs;
    if (pool == NULL) {
        log_warn("the achievement set has no memory references");
        return 0;
    }

    for (ml = &pool->memrefs; ml != NULL; ml = ml->next) {
        uint16_t k;

        for (k = 0; k < ml->count; k++) {
            rc_memref_t *m = &ml->items[k];
            int b = memsize_bytes(rc_memref_shared_size(m->value.size));

            if (n >= RA_WATCH_MAX) {
                log_warn("more than %d addresses, the watch list does not fit", RA_WATCH_MAX);
                return 0;
            }
            if (off + b > RA_SNAP_MAX_BYTES) {
                log_warn("snapshot exceeds %d bytes, it does not fit in a packet", RA_SNAP_MAX_BYTES);
                return 0;
            }

            g_watch[n] = RA_WATCH_PACK(m->address, (unsigned int)b);
            g_offset[n] = (unsigned int)off;
            g_direct_of[n] = m;
            off += b;
            n++;
        }
    }

    if (n == 0) {
        log_warn("the achievement set has no direct memory reads");
        return 0;
    }

    g_count = n;
    g_bytes = off;
    log_info("watch list: %d addresses, %d bytes per snapshot", n, off);

    build_nodes(client);
    count_indirect(client);
    return 1;
}

int watchlist_count(void)
{
    return g_count;
}

int watchlist_bytes(void)
{
    return g_bytes;
}

int watchlist_node_count(void)
{
    return g_node_count;
}

int watchlist_snapshot_bytes(void)
{
    return g_bytes + g_node_count * RA_NODE_PAIR_BYTES;
}

int watchlist_serialize(unsigned char *out, size_t cap)
{
    struct ra_watch_file hdr;
    struct ra_node_file nhdr;
    size_t need = sizeof(hdr) + (size_t)g_count * sizeof(unsigned int);
    size_t at;
    int i;

    if (g_node_count > 0)
        need += sizeof(nhdr) + (size_t)g_node_count * sizeof(struct ra_node);

    if (g_count == 0 || need > cap)
        return 0;

    hdr.magic = RA_WATCH_MAGIC;
    hdr.version = RA_WATCH_VERSION;
    hdr.count = (unsigned int)g_count;
    /* Direct values only: this is what every console build ever shipped
       reads it as, and one that follows pointers adds the pairs itself. */
    hdr.bytes = (unsigned int)g_bytes;
    memcpy(out, &hdr, sizeof(hdr));
    at = sizeof(hdr);

    for (i = 0; i < g_count; i++, at += sizeof(unsigned int))
        memcpy(out + at, &g_watch[i], sizeof(unsigned int));

    if (g_node_count > 0) {
        nhdr.magic = RA_NODE_MAGIC;
        nhdr.count = (unsigned int)g_node_count;
        memcpy(out + at, &nhdr, sizeof(nhdr));
        at += sizeof(nhdr);

        for (i = 0; i < g_node_count; i++, at += sizeof(struct ra_node))
            memcpy(out + at, &g_nodes[i], sizeof(struct ra_node));
    }

    return (int)need;
}

unsigned char *watchlist_values(void)
{
    return g_values;
}

void watchlist_set_have_values(int have)
{
    g_have_values = have;
}

void watchlist_set_have_nodes(int have)
{
    g_have_nodes = have;
}

/* Must return exactly num_bytes or nothing. A short read of a direct
   memref makes rcheevos disable every achievement using it for the rest
   of the session.

   Before the first snapshot rcheevos probes each memref once to check
   that the address is readable. Answering "unreadable" would disable
   the achievements, so zeros are returned with a full count. */
uint32_t watchlist_read_memory(uint32_t address, uint8_t *buffer, uint32_t num_bytes,
                               rc_client_t *client)
{
    int i;

    (void)client;

    if (!g_have_values) {
        memset(buffer, 0, num_bytes);
        return num_bytes;
    }

    /* Linear search over a few hundred entries is cheap on a PC, and a
       sorted index would complicate the order mapping to the snapshot. */
    for (i = 0; i < g_count; i++) {
        unsigned int a = RA_WATCH_ADDR(g_watch[i]);
        unsigned int n = RA_WATCH_SIZE(g_watch[i]);

        if (address >= a && address + num_bytes <= a + n) {
            unsigned int off = g_offset[i] + (address - a);

            if (off + num_bytes > (unsigned int)g_bytes)
                return 0;

            memcpy(buffer, &g_values[off], num_bytes);
            return num_bytes;
        }
    }

    /* Not a fixed address: rcheevos has walked a pointer chain and is
       asking for what it landed on. The console walked the same chain in
       the same frame and says where it ended up, so the answer is a
       lookup by that address rather than a second walk here that could
       disagree with it. */
    if (g_have_nodes) {
        const unsigned char *pair = &g_values[g_bytes];

        for (i = 0; i < g_node_count; i++, pair += RA_NODE_PAIR_BYTES) {
            unsigned int a = load_le32(pair);
            unsigned int n = RA_NODE_SIZE(g_nodes[i].w);

            /* Address 0: the chain led nowhere this frame. */
            if (a == 0)
                continue;

            if (address >= a && address + num_bytes <= a + n) {
                unsigned int v = load_le32(pair + 4);
                unsigned char b[4];

                b[0] = (unsigned char)v;
                b[1] = (unsigned char)(v >> 8);
                b[2] = (unsigned char)(v >> 16);
                b[3] = (unsigned char)(v >> 24);
                memcpy(buffer, b + (address - a), num_bytes);
                return num_bytes;
            }
        }
    }

    return 0;
}
