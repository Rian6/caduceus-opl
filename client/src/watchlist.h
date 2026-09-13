/*
  The watch list: which addresses the console reads every frame, in
  which order, and the sparse memory image built from its snapshots.
*/
#ifndef XERABORA_WATCHLIST_H
#define XERABORA_WATCHLIST_H

#include <stddef.h>
#include <stdint.h>

#include "rc_client.h"

/* Builds the list from the memrefs of the game loaded in rc_client.
   Returns 1 on success. */
int watchlist_build(rc_client_t *client);

int watchlist_count(void);
/* Achievements reading through a pointer chain the console cannot be
   asked to follow: active, but they cannot unlock. Reported as
   "unsupported". Chains that compiled into nodes are not counted. */
int watchlist_indirect_count(void);
/* Pointer chains the console resolves every frame. */
int watchlist_node_count(void);
/* Direct values in a snapshot. */
int watchlist_bytes(void);
/* Direct values plus one (address, value) pair per node: what a console
   that follows pointers sends. Equal to watchlist_bytes() when the set
   has no chains. */
int watchlist_snapshot_bytes(void);

/* Serialises the list in the on-wire/file format the console expects.
   Returns the number of bytes written, 0 if it does not fit. */
int watchlist_serialize(unsigned char *out, size_t cap);

/* Snapshot values, in list order. */
unsigned char *watchlist_values(void);
void watchlist_set_have_values(int have);
/* Whether the last snapshot carried the node pairs. An older console
   sends direct values only. */
void watchlist_set_have_nodes(int have);

/* rc_client memory callback backed by the last snapshot. */
uint32_t watchlist_read_memory(uint32_t address, uint8_t *buffer, uint32_t num_bytes,
                               rc_client_t *client);

#endif
