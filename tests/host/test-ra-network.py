"""Exercise the real RA socket setup with deterministic network failures."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "opl/src/ranet.c").read_text()
start = source.index("static int open_pc_socket(")
end = source.index("/* A telemetry launch", start)
test = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
typedef unsigned char u8;
#define RA_MY_PORT 18196
#define LOG(...) ((void)0)
static void raHashStep(const char *s) {}
static int ready, linkUp, dhcp, ps2_ip_use_dhcp = 1, initFail, bindFail;
static int inits, closes;
static int ethLoadInitModules(void) {
    inits++;
    if (initFail) return -1;
    ready = linkUp = dhcp = 1;
    return 0;
}
static int ethGetNetIFLinkStatus(void) { assert(ready); return linkUp; }
static int ethGetDHCPStatus(void) { return dhcp; }
static int ethGetNetConfig(u8 *ip, u8 *mask, u8 *gw) {
    memset(ip, 0, 4);
    if (dhcp) { ip[0]=192; ip[1]=168; ip[2]=1; ip[3]=8; }
    return 0;
}
static int mock_socket(int a,int b,int c) { return ready ? 7 : -1; }
static int mock_bind(int a,const struct sockaddr *b,socklen_t c) { return bindFail ? -1 : 0; }
static int mock_setsockopt(int a,int b,int c,const void *d,socklen_t e) { return 0; }
static int mock_fcntl(int a,int b,...) { return 0; }
static int disconnect(int fd) { closes++; return 0; }
#define socket mock_socket
#define bind mock_bind
#define setsockopt mock_setsockopt
#define fcntl mock_fcntl
'''
test += source[start:end]
test += r'''
int main(void) {
    char addr[32]; u8 ip[4];
    assert(open_pc_socket(addr, sizeof(addr), ip) == 7);
    assert(inits == 1 && !strcmp(addr, " 192.168.1.8 18196"));
    assert(open_pc_socket(addr, sizeof(addr), ip) == 7 && inits == 1);
    linkUp = dhcp = 0;
    assert(open_pc_socket(addr, sizeof(addr), ip) == 7 && inits == 2);
    bindFail = 1;
    assert(open_pc_socket(addr, sizeof(addr), ip) == -1 && closes == 1);
    bindFail = 0; linkUp = 0; initFail = 1;
    assert(open_pc_socket(addr, sizeof(addr), ip) == -1 && closes == 2);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="opl-ra-network-") as directory:
    path = Path(directory)
    (path / "test.c").write_text(test)
    subprocess.run(["gcc", str(path / "test.c"), "-o", str(path / "test")], check=True)
    subprocess.run([str(path / "test")], check=True)
print("PASS: cold network, healthy reuse, link recovery, bind failure and failed recovery cleanup")
