"""Host checks for card result identity and worker publication; no live RA account."""
from pathlib import Path
import subprocess
import tempfile
import os

root = Path(__file__).resolve().parents[2]
source = (root / "opl/src/supportbase.c").read_text()
header = (root / "opl/include/supportbase.h").read_text()
types = header[header.index("enum sb_ra_check_state"):header.index("/* GUI-only snapshot")]
start = source.index("static char ra_hash_path")
end = source.index("/* One test at a time, like the image check", start)
test = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define ISO_GAME_NAME_MAX 160
#define IO_CUSTOM_SIMPLEACTION 1
static int queued;
static int achievementsBusy(void) { return 0; }
static void ioPutRequest(int kind, void *worker) { queued++; }
void sbHashGame(const char *, const char *, const char *, const char *, int);
'''
test += types + source[start:end]
test += r'''
static int nextState = SB_RA_SUPPORTED;
void sbHashGame(const char *p, const char *n, const char *e, const char *s, int f) {
    ra_check_result.state = nextState;
    strcpy(ra_check_result.hash, "0123456789abcdef0123456789abcdef");
    strcpy(ra_check_result.title, "Bully");
}
int main(void) {
    sb_ra_check_result_t result;
    const char *p = "smb0:", *n = "Bully (USA)", *e = ".iso", *s = "SLUS_212.69";
    assert(!sbGetGameCheck(p,n,e,s,2,&result));
    assert(result.state == SB_RA_UNCHECKED);
    assert(sbHashGameDeferred(p,n,e,s,2) && sbGameCheckBusy());
    assert(sbGetGameCheck(p,n,e,s,2,&result) && result.state == SB_RA_CHECKING);
    assert(!result.hash[0]);
    assert(!sbHashGameDeferred(p,"Another game",e,s,2) && queued == 1);
    assert(!sbGetGameCheck("mass0:",n,e,s,2,&result));
    assert(!sbGetGameCheck(p,n,e,s,1,&result));
    sbHashGameDeferredWorker();
    assert(!sbGameCheckBusy());
    assert(sbGetGameCheck(p,n,e,s,2,&result) && result.state == SB_RA_SUPPORTED);
    assert(strlen(result.hash) == 32);
    assert(!sbGetGameCheck(p,"Another game",e,s,2,&result));
    assert(result.state == SB_RA_UNCHECKED && !result.hash[0]);
    nextState = SB_RA_ERROR;
    assert(sbHashGameDeferred(p,n,e,s,2));
    assert(sbGetGameCheck(p,n,e,s,2,&result) && !result.hash[0]);
    sbHashGameDeferredWorker();
    assert(sbGetGameCheck(p,n,e,s,2,&result) && result.state == SB_RA_ERROR);
    char longName[161]; memset(longName,'X',160); longName[160] = 0;
    assert(sbHashGameDeferred(p,longName,e,s,2));
    assert(sbGetGameCheck(p,longName,e,s,2,&result));
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="opl-ra-card-") as directory:
    path = Path(directory)
    (path / "test.c").write_text(test)
    executable = path / "test"
    subprocess.run([os.environ.get("CC", "gcc"), "-std=c99", "-Wall", "-Werror",
                    str(path / "test.c"), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
print("PASS: pending result, duplicate request, full image identity, retry, error and long filename")
