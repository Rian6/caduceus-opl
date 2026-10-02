#include "include/opl.h"
#include "include/achievements.h"
#include "include/ranet.h"
#include "include/ioman.h"

static volatile int busy;
static achievement_page_t result;
static char request[180];
static unsigned int serial;

/* Empty fields are significant; strtok would merge them. */
static char *field(char **cursor, char separator)
{
    char *start = *cursor, *end;
    if (!start) return NULL;
    end = strchr(start, separator);
    if (end) { *end = 0; *cursor = end + 1; }
    else *cursor = NULL;
    return start;
}

static int integer(char *text, int *value)
{
    char *end;
    if (!text || !*text) return 0;
    long n = strtol(text, &end, 10);
    if (*end || n < 0 || n > 10000000) return 0;
    *value = n;
    return 1;
}

int achievementsParse(char *reply, achievement_page_t *out)
{
    char *lines = reply, *header = field(&lines, '\n'), *part;
    memset(out, 0, sizeof(*out));
    out->state = ACH_ERROR;
    if (!strcmp(header, "OFFLINE")) { out->state = ACH_OFFLINE; return 1; }
    if (!strcmp(header, "UNSUPPORTED")) { out->state = ACH_UNSUPPORTED; return 1; }
    part = field(&header, '\t');
    if (!part || strcmp(part, "OK")) return 0;
    part = field(&header, '\t');
    if (!part || strlen(part) != 1 || (*part != 'G' && *part != 'A')) return 0;
    out->kind = *part;
    if (!integer(field(&header, '\t'), &out->page) ||
        !integer(field(&header, '\t'), &out->total) ||
        !integer(field(&header, '\t'), &out->gameId) ||
        !integer(field(&header, '\t'), &out->earned) ||
        !integer(field(&header, '\t'), &out->maximum)) return 0;
    part = field(&header, '\t'); if (!part || strlen(part) > 24) return 0;
    snprintf(out->user, sizeof(out->user), "%s", part);
    part = field(&header, '\t'); if (!part || strlen(part) > 56 || header) return 0;
    snprintf(out->title, sizeof(out->title), "%s", part);
    while (lines && *lines) {
        char *row = field(&lines, '\n');
        if (out->count == ACH_PAGE_SIZE) return 0;
        achievement_entry_t *entry = &out->entries[out->count++];
        if (!integer(field(&row, '\t'), &entry->id) || !entry->id ||
            !integer(field(&row, '\t'), &entry->total) ||
            !integer(field(&row, '\t'), &entry->earned) ||
            !integer(field(&row, '\t'), &entry->hardcore) ||
            !integer(field(&row, '\t'), &entry->points)) return 0;
        part = field(&row, '\t'); if (!part) return 0;
        if (strcmp(part, "-")) {
            if (strlen(part) != 32 || strspn(part, "0123456789abcdef") != 32) return 0;
            snprintf(entry->icon, sizeof(entry->icon), "%s", part);
        }
        part = field(&row, '\t'); if (!part || strlen(part) > 56) return 0;
        snprintf(entry->title, sizeof(entry->title), "%s", part);
        part = field(&row, '\t'); if (!part || strlen(part) > 100) return 0;
        snprintf(entry->description, sizeof(entry->description), "%s", part);
        part = field(&row, '\t'); if (!part || strlen(part) > 19 || row) return 0;
        snprintf(entry->date, sizeof(entry->date), "%s", part);
    }
    if (out->count > out->total || out->earned > out->maximum) return 0;
    out->state = ACH_READY;
    return 1;
}

static void loadPage(void)
{
    static char response[1024];
    memset(&result, 0, sizeof(result));
    result.state = ACH_ERROR;
    char key[65];
    int fd = open("smb0:ART/CADUCEUS.KEY", O_RDONLY);
    int length = fd >= 0 ? read(fd, key, 64) : -1;
    if (fd >= 0) close(fd);
    if (length != 64) { result.state = ACH_OFFLINE; goto done; }
    key[64] = 0;
    if (strspn(key, "0123456789abcdef") != 64) { result.state = ACH_OFFLINE; goto done; }
    strncat(request, " ", sizeof(request) - strlen(request) - 1);
    strncat(request, key, sizeof(request) - strlen(request) - 1);
    memset(key, 0, sizeof(key));
    if (raCaduceusPage(request, serial, response, sizeof(response)) == 0 &&
        !achievementsParse(response, &result)) result.state = ACH_ERROR;
done:
    memset(key, 0, sizeof(key));
    memset(request, 0, sizeof(request));
    __asm__ volatile("" ::: "memory");
    busy = 0;
}

int achievementsBusy(void) { return busy; }

int achievementsRequest(char kind, int page, int filter, const char *target)
{
    if (sbGameCheckBusy() || page < 0 || page > 999999 || filter < 0 || filter > 3 ||
        (kind != 'G' && kind != 'A') || !target || !*target || strlen(target) > 32 ||
        strspn(target, "0123456789abcdef") != strlen(target)) return 0;
    busy = 1;
    serial++;
    snprintf(request, sizeof(request), "CADA1 %u %c %d %d %s", serial, kind, page, filter, target);
    if (ioPutRequest(IO_CUSTOM_SIMPLEACTION, &loadPage) < 0) { busy = 0; return 0; }
    return 1;
}

void achievementsSnapshot(achievement_page_t *out)
{
    memset(out, 0, sizeof(*out));
    if (busy) { out->state = ACH_LOADING; return; }
    __asm__ volatile("" ::: "memory");
    *out = result;
}
