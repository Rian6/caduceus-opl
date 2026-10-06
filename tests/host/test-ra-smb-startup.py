"""Execute IOP discovery/startup: SMB must never use a receive socket."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'opl/modules/network/raudp/raudp.c').read_text()
discovery=s[s.index('#define RA_DISC_POLL_US'):s.index('/* ---- Messages from the PC')]
thread=s[s.index('static void ra_thread('):s.index('int _shutdown(')]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <netinet/in.h>
typedef uint32_t u32;typedef uint8_t u8;
static int ra_rx_in_game,ra_sock=-1,sockets,delays,arp_calls,arp_ready;
static u32 ra_server_ip=0x5101a8c0,ra_gateway_ip,ra_dst_ip,ra_src_ip;
static u8 ra_dst_mac[6];static unsigned int first_delay;
static jmp_buf running;
#define RA_DST_PORT 18194
#define RA_SRC_PORT 18195
#define RA_QUIET_US 30000000
#define RA_IDLE_TICKS 4
#define RA_KEEPALIVE_TICKS 250
#define RA_HEARTBEAT_TICKS 2500
#define RA_POLL_US 4000
static void DelayThread(unsigned int us){if(!delays++)first_delay=us;}
static int etharp_lookup_mac(u32 ip,u8 *mac){
 arp_calls++;if(arp_ready&&ip==ra_server_ip){memset(mac,0x11,6);return 1;}return 0;
}
static int lwip_socket(int a,int b,int c){sockets++;return -1;}
#define lwip_setsockopt(...) (assert(0),0)
#define lwip_bind(...) (assert(0),0)
#define lwip_close(...) (assert(0),0)
#define lwip_sendto(...) (assert(0),0)
#define lwip_recvfrom(...) (assert(0),0)
static int ra_fmt_ip(char *p,u32 ip){return 0;}
static void ra_fmt(u8 *p,int a,int b){}
static void ra_frame_init(void){assert(first_delay==RA_QUIET_US);}
static int ra_snap_pending(void){assert(first_delay==RA_QUIET_US);longjmp(running,1);}
static void ra_send_one(void){assert(0);}
static int SMAPReadNotice(char *p,int n){assert(0);return 0;}
static void ra_handle_pc(char *p,int n){assert(0);}
static void ra_drain_rx(void){assert(0);}
static void ra_poll_pc(void){assert(0);}
static void ra_heartbeat(void){assert(0);}
'''+discovery+thread+r'''
int main(void){
 arp_ready=1;assert(ra_discover()==1&&ra_dst_ip==ra_server_ip&&!sockets&&ra_sock==-1);
 arp_ready=0;arp_calls=delays=0;assert(!ra_discover());assert(delays==20&&!sockets);
 ra_server_ip=0;arp_calls=delays=0;ra_thread(NULL);
 assert(first_delay==RA_QUIET_US&&!arp_calls&&!sockets);
 ra_server_ip=0x5101a8c0;arp_ready=1;delays=0;
 if(!setjmp(running))ra_thread(NULL);
 assert(first_delay==RA_QUIET_US&&delays==1&&!sockets);
 ra_rx_in_game=1;assert(!ra_discover()&&sockets==1);
 puts("PASS: SMB discovery uses only ARP; missing host/MAC exits; quiet period precedes network; non-SMB socket path retained");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['gcc','-std=gnu99',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
