"""Host regression tests for the real IOP SMB session teardown functions.

Run with Python and a host C compiler (CC, or gcc). No PS2 or live share needed.
"""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "opl/modules/network/smbman-ra/smb_fio.c").read_text()


def function(name, argument):
    signature = f"static int {name}({argument})\n{{"
    start = source.index(signature)
    end = source.index("\n}", start) + 2
    return source[start:end]


test = r'''
#include <assert.h>
#include <errno.h>
#include <string.h>
typedef unsigned int u32;
typedef struct { char serverIP[16], User[32], Password[32]; int serverPort, PasswordType; } smbLogOn_in_t;
enum { SMB_DEVCTL_LOGON_ERR_CONN=1, SMB_DEVCTL_LOGON_ERR_PROT, SMB_DEVCTL_LOGON_ERR_LOGON };
static int UID, TID, closes, locked, treeResult, logoffResult, negotiationResult, loginResult;
static smbLogOn_in_t glogon_info;
static void keepalive_lock(void) { locked = 1; }
static void keepalive_unlock(void) { locked = 0; }
static int smb_Disconnect(void) { closes++; return 0; }
static void smb_closeAll(void) {}
static int smb_TreeDisconnect(int u, int t) { return treeResult; }
static int smb_LogOffAndX(int u) { return logoffResult; }
static int smb_Connect(char *ip, int port) { return 0; }
static int smb_NegotiateProtocol(u32 *caps) { *caps = 0; return negotiationResult; }
static int smb_SessionSetupAndX(char *u, char *p, int type, u32 caps) { return loginResult; }
'''
test += function("smb_LogOff", "void")
test += function("smb_CloseShare", "void")
test += function("smb_LogOn", "smbLogOn_in_t *logon")
test += r'''
int main(void) {
    smbLogOn_in_t logon = {0};
    UID = 7; TID = 8; treeResult = logoffResult = -EIO;
    assert(smb_LogOff() == -EIO);
    assert(UID == -1 && TID == -1 && closes == 1 && locked);

    closes = 0;
    assert(smb_LogOff() == -ENOTCONN);
    assert(closes == 1 && locked);

    UID = 7; TID = 8;
    assert(smb_CloseShare() == -EIO);
    assert(TID == -1);

    UID = -1; closes = 0; negotiationResult = -EIO;
    assert(smb_LogOn(&logon) == -SMB_DEVCTL_LOGON_ERR_PROT);
    assert(closes == 1 && UID == -1);

    closes = 0; negotiationResult = 0; loginResult = -EACCES;
    assert(smb_LogOn(&logon) == -SMB_DEVCTL_LOGON_ERR_LOGON);
    assert(closes == 1 && UID == -1);

    closes = 0; loginResult = 42;
    assert(smb_LogOn(&logon) == 0);
    assert(UID == 42 && !locked && closes == 0);

    TID = 8; treeResult = logoffResult = 0;
    assert(smb_LogOff() == 0);
    assert(UID == -1 && TID == -1 && closes == 1 && locked);
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="opl-smb-test-") as directory:
    path = Path(directory)
    (path / "test.c").write_text(test)
    executable = path / "test"
    subprocess.run([os.environ.get("CC", "gcc"), "-std=c99", "-Wall", "-Werror",
                    str(path / "test.c"), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
print("PASS: failed logoff/share close, partial login cleanup, reconnect and clean logout")

# Compile the real transport teardown without a recv stub: a blocking drain must
# not be reintroduced when recovering from a server that never sends its FIN.
transport = (root / "opl/modules/network/smbman-ra/smb.c").read_text()
start = transport.index("int smb_Disconnect(void)\n{")
end = transport.index("\n}", start) + 2
test = r'''
#include <assert.h>
enum { SHUT_RDWR = 2 };
static int main_socket = 9, shutdowns, closes;
static int shutdown(int fd, int how) { assert(fd == 9 && how == SHUT_RDWR); shutdowns++; return -1; }
static int lwip_close(int fd) { assert(fd == 9); closes++; return 0; }
'''
test += transport[start:end]
test += r'''
int main(void) {
    assert(smb_Disconnect() == 0);
    assert(main_socket == -1 && shutdowns == 1 && closes == 1);
    assert(smb_Disconnect() == 0);
    assert(shutdowns == 1 && closes == 1);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="opl-smb-transport-") as directory:
    path = Path(directory)
    (path / "test.c").write_text(test)
    executable = path / "test"
    subprocess.run([os.environ.get("CC", "gcc"), "-std=c99", "-Wall", "-Werror",
                    str(path / "test.c"), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
print("PASS: transport closes after failed shutdown, with no peer-FIN wait; repeated close is safe")
