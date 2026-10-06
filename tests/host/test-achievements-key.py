"""Exercise the console key reader without SMB or account credentials."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'opl/src/achievements.c').read_text()
loader = source[source.index('static void loadPage(void)'):source.index('int achievementsBusy(void)')]
code = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define O_RDONLY 0
''' + (root / 'opl/include/achievements.h').read_text() + r'''
static int busy, calls, missing;
static unsigned int serial = 7;
static achievement_page_t result;
static char request[180], fixture[80];
static int open(const char *path, int flags) {
    assert(!strcmp(path, "smb0:ART/CADUCEUS.KEY")); return missing ? -1 : 1;
}
static int read(int fd, void *out, unsigned int size) {
    unsigned int n = strlen(fixture); if (n > size) n = size;
    memcpy(out, fixture, n); return n;
}
static int close(int fd) { return 0; }
static int raCaduceusPage(const char *req, unsigned int nonce, char *out, int size) {
    calls++; assert(nonce == 7);
    assert(strlen(req) == strlen("CADA1 7 G 0 0 0 ") + 64);
    assert(strspn(req + strlen("CADA1 7 G 0 0 0 "), "a") == 64);
    strcpy(out, "OFFLINE"); return 0;
}
int achievementsParse(char *reply, achievement_page_t *out) {
    assert(!strcmp(reply, "OFFLINE")); out->state = ACH_OFFLINE; return 1;
}
''' + loader + r'''
static void run(void) {
    strcpy(request, "CADA1 7 G 0 0 0"); busy = 1; loadPage();
    assert(!busy && !request[0]);
}
int main(void) {
    memset(fixture, 'a', 64); run(); assert(calls == 1);
    /* The bounded read ignores a newline after a complete 64-byte key. */
    strcpy(fixture + 64, "\r\n");
    run(); assert(calls == 2);
    missing = 1; run(); assert(calls == 2 && result.state == ACH_OFFLINE);
    missing = 0; strcpy(fixture, "abc\r\n\t ");
    run(); assert(calls == 2 && result.state == ACH_OFFLINE);
    memset(fixture, 'g', 64); fixture[64] = 0;
    run(); assert(calls == 2 && result.state == ACH_OFFLINE);
    return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / 'test.c').write_text(code)
    subprocess.run(['gcc', '-std=gnu99', '-Wall', '-Werror', str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print('PASS: console key reader, invalid/missing keys, request cleanup and single-character escapes')
