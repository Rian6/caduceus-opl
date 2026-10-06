"""Test the actual RAU parser and SMB receive tap with host IOP stubs."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'opl/modules/network/raudp/raudp.c').read_text()
parser=s[s.index('static u32 ra_dec_at'):s.index('/* The lwIP road')]
s=(root/'opl/modules/network/smap-ingame/xfer.c').read_text()
tap=s[s.index('static unsigned char ra_notice'):s.index('int HandleRxIntr')]
s=(root/'opl/modules/network/common/ra_snap.h').read_text()
events=s[s.index('#define RA_EVENT_MAGIC'):s.index('/* The load argument')]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef uint32_t u32;
typedef uint8_t u8;
typedef struct {int value;} iop_sys_clock_t;
typedef struct {void *src,*dest;unsigned int size,attr;} SifDmaTransfer_t;
static void CpuSuspendIntr(int *s){*s=0;}
static void CpuResumeIntr(int s){}
static void GetSystemTime(iop_sys_clock_t *c){}
static void SysClock2USec(iop_sys_clock_t *c,u32 *s,u32 *u){*s=100;*u=0;}
static int sceSifDmaStat(int id){return -1;}
static int dma_count;
static int sceSifSetDma(SifDmaTransfer_t *d,int n){assert(d->size==128);dma_count++;return 1;}
static void ra_fmt(void *a,u32 v,int n){}
static void ra_fmt_hex8(void *a,u32 v){}
static void ra_fmt_err(void *a,int v){}
static void ra_ctl_send(void *a,int n){}
static unsigned int ra_hb_rx,ra_hb_rau,ra_ee_event=0x1000;
'''+events+'\nstatic struct ra_event ra_event;\n'+parser+tap+r'''
static void send(const char *text){char s[256];strcpy(s,text);ra_handle_pc(s,strlen(s));}
int main(void){
 send("RAU1 123 1");assert(ra_event.arg==123&&RA_EVENT_POINTS(ra_event.kind)==1&&!ra_event.title[0]);
 send("RAU1 123 1 Get Off, You Psycho!");assert(!strcmp(ra_event.title,"Get Off, You Psycho!")&&dma_count==2);
 send("RAU1 123 1 Get Off, You Psycho!");assert(dma_count==2);
 send("RAU1 124 5 Second unlock");assert(ra_event.arg==124&&ra_event.commit==ra_event.seq);
 send("RAU1 invalid");assert(ra_event.arg==124);
 unsigned char frame[256]={0},out[128];char *body="RAU1 125 2 SMB unlock";unsigned n=strlen(body);
 frame[12]=8;frame[14]=0x45;frame[23]=17;frame[36]=18195>>8;frame[37]=18195&255;
 frame[39]=n+8;memcpy(frame+42,body,n);
 ra_tap_notice(frame,n+42);assert(SMAPReadNotice(out,128)==(int)n);assert(!memcmp(out,body,n));
 assert(!SMAPReadNotice(out,128));
 frame[37]=1;ra_tap_notice(frame,n+42);assert(!SMAPReadNotice(out,128));
 frame[37]=18195&255;frame[39]=130;ra_tap_notice(frame,n+42);assert(!SMAPReadNotice(out,128));
 puts("PASS: legacy/enriched unlocks, title, deduplication, 128-byte DMA, SMB tap and malformed bounds");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['gcc','-std=gnu99','-Wno-int-to-pointer-cast',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
