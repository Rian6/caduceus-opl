"""Execute the actual overlay C with GS registers and write-shadow stubs."""
from pathlib import Path
import re,subprocess,tempfile,sys
root=Path(__file__).resolve().parents[2]
s=(root/'opl/ee_core/src/ra_overlay.c').read_text()
s=re.sub(r'^#include .*\n','',s,flags=re.M)
for name in ['GIF_D2_CHCR','GIF_D2_MADR','GIF_D2_QWC','GIF_STAT']:
 s=re.sub(r'^#define '+name+r' .*$','',s,flags=re.M)
events=(root/'opl/modules/network/common/ra_snap.h').read_text()
events=events[events.index('#define RA_EVENT_MAGIC'):events.index('/* The load argument')]
features=(root/'opl/modules/network/common/ra_features.h').read_text()
code=features+r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint64_t u64;typedef uint32_t u32;typedef uint8_t u8;
#define UNCACHED_SEG(p) (p)
static u32 GIF_D2_CHCR,GIF_D2_MADR,GIF_D2_QWC,GIF_STAT;
static int resets;static unsigned int flushed;
static void IGR_RequestReset(void){resets++;}
static void iSyncDCache(void *a,void *b){flushed+=(char*)b-(char*)a;}
'''+events+s+r'''
struct ra_gs_source_regs GSMSourceGSRegs;
static void validate_packet(u64 *p){
 assert((p[0]&0x7fff)==4 && !(p[0]&(1<<15)));
 assert(p[3]==GS_REG_BITBLTBUF&&p[5]==GS_REG_TRXPOS&&p[7]==GS_REG_TRXREG&&p[9]==GS_REG_TRXDIR);
 assert((p[10]>>58&3)==2 && (p[10]&(1<<15)));
 assert((p[10]&0x7fff)+6==GIF_D2_QWC);
 assert(GIF_D2_QWC*16<=RA_PACKET_BYTES);
}
static void finish_card(unsigned int frame){
 for(int i=0;i<RA_CARD_H/RA_BUILD_ROWS+1;i++){
  unsigned int before=flushed;
  RA_OverlayOnVblank(frame);
  assert(flushed-before<=RA_BUILD_ROWS*RA_CARD_W*4+128);
  if(!ra_dirty)return;
  assert(!GIF_D2_CHCR);
 }
 assert(0);
}
int main(void){
 u64 packet[(RA_PACKET_BYTES+15)/8+8]={0};
 struct ra_event *e=RA_OverlayEventBuffer();RA_OverlaySetPacketBuffer(packet);
 GSMSourceGSRegs.pmode=1;GSMSourceGSRegs.dispfb1=(u64)10<<9;
 GSMSourceGSRegs.display1=(u64)639<<32;
 RA_OverlayOnVblank(99);assert(!ra_ovl_running&&!GIF_D2_CHCR);
 e->magic=RA_EVENT_MAGIC;e->seq=e->commit=1;e->kind=RA_EVENT_MAKE_UNLOCK(1);e->arg=12345;
 strcpy(e->title,"Get Off, You Psycho!");finish_card(100);
 assert(ra_ovl_running&&ra_ovl_points==1&&ra_pixel_bytes==4);validate_packet(packet);
 printf("IMAGE %u bytes; no drawing-context registers\n",GIF_D2_QWC*16);
 u32 *pixels=(u32*)((char*)packet+96);
 unsigned int x=(packet[4]>>32)&0x7ff,y=(packet[4]>>48)&0x7ff;
 for(int row=0;row<RA_CARD_H;row++)for(int col=0;col<RA_CARD_W;){
  int end=col+1;while(end<RA_CARD_W&&pixels[row*RA_CARD_W+end]==pixels[row*RA_CARD_W+col])end++;
  printf("RECT %u %u %u %u %06x\n",x+col,y+row,x+end,y+row+1,pixels[row*RA_CARD_W+col]&0xffffff);col=end;
 }
 u64 saved=packet[2];ra_dirty=1;GIF_D2_CHCR=0x100;RA_OverlayOnVblank(120);assert(packet[2]==saved&&ra_dirty);
 GIF_D2_CHCR=0;GIF_STAT=1<<6;RA_OverlayOnVblank(121);assert(packet[2]==saved&&ra_dirty);
 GIF_STAT=0;finish_card(122);assert(!ra_dirty);
 flushed=0;GIF_D2_CHCR=0;GSMSourceGSRegs.dispfb1|=7;RA_OverlayOnVblank(123);
 assert(((packet[2]>>32)&0x3fff)==7*32&&flushed==128);validate_packet(packet);
 e->seq=2;e->commit=1;RA_OverlayOnVblank(124);assert(ra_ovl_seen==1);e->seq=1;
 GIF_D2_CHCR=0;RA_OverlayOnVblank(309);assert(ra_ovl_running);GIF_D2_CHCR=0;RA_OverlayOnVblank(310);assert(!ra_ovl_running&&!GIF_D2_CHCR);
 e->seq=e->commit=2;GSMSourceGSRegs.dispfb1=((u64)10<<9)|((u64)2<<15);
 GIF_D2_CHCR=0;finish_card(400);assert(ra_pixel_bytes==2);validate_packet(packet);
 assert(((unsigned short*)((char*)packet+96))[0]==((0xd3>>3)|((0xf2>>3)<<5)|((0x72>>3)<<10)|0x8000));
 e->seq=e->commit=3;GSMSourceGSRegs.dispfb1=((u64)10<<9)|((u64)19<<15);
 GIF_D2_CHCR=0;RA_OverlayOnVblank(440);assert(ra_ovl_running&&!GIF_D2_CHCR);
 GSMSourceGSRegs.dispfb1=((u64)10<<9)|((u64)1<<15);finish_card(441);validate_packet(packet);
 assert(((packet[2]>>56)&0x3f)==0); /* CT24 uses identical CT32 storage, 4-byte IMAGE */
 e->seq=e->commit=4;e->kind=RA_EVENT_RESET;RA_OverlayOnVblank(500);assert(resets==1);
 RA_OverlayOnVblank(501);assert(resets==1);
 e->seq=e->commit=5;e->kind=RA_EVENT_MAKE_UNLOCK(25);
 memset(e->title,'W',63);e->title[63]=0;
 GIF_D2_CHCR=0;finish_card(502);validate_packet(packet);
 assert(ra_ovl_points==25&&!strncmp(ra_ovl_title,e->title,63));
 /* Format change without a new unlock must also keep the IMAGE unsubmitted. */
 GIF_D2_CHCR=0;GSMSourceGSRegs.dispfb1=((u64)10<<9)|((u64)2<<15);
 RA_OverlayOnVblank(503);assert(ra_dirty&&ra_build_row==RA_BUILD_ROWS&&!GIF_D2_CHCR);
 unsigned int before=flushed;GIF_STAT=1<<6;RA_OverlayOnVblank(504);
 assert(ra_build_row==RA_BUILD_ROWS&&flushed==before&&!GIF_D2_CHCR);
 GIF_STAT=0;finish_card(505);validate_packet(packet);
 /* A newer notice during a partial build restarts it; reset remains immediate. */
 e->seq=e->commit=6;strcpy(e->title,"First pending");
 GIF_D2_CHCR=0;RA_OverlayOnVblank(506);assert(ra_dirty&&!GIF_D2_CHCR);
 e->seq=e->commit=7;strcpy(e->title,"New pending");
 RA_OverlayOnVblank(507);assert(ra_build_row==RA_BUILD_ROWS&&!GIF_D2_CHCR);
 e->seq=e->commit=8;e->kind=RA_EVENT_RESET;RA_OverlayOnVblank(508);assert(resets==2);

 puts("PASS: IMAGE packet bounds, RGB/CT16/CT24, busy GIF paths, buffer changes, retry, DMA commit and reset");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['gcc','-DRA_ENABLE_EXPERIMENTAL_CARD=1','-std=gnu99','-Wall','-Wno-pointer-to-int-cast',str(p/'test.c'),'-o',str(p/'test')],check=True)
 output=subprocess.check_output([str(p/'test')],text=True)
 if '--preview' in sys.argv:(root/'pcsx2-test/ra-unlock-card-primitives.txt').write_text(output)
 print(output.splitlines()[0]);print(output.splitlines()[-1])
 disabled=code.split('int main(void){')[0]+r'''
int main(void){
 u64 packet[(RA_PACKET_BYTES+15)/8+8]={0};
 struct ra_event *e=RA_OverlayEventBuffer();RA_OverlaySetPacketBuffer(packet);
 GSMSourceGSRegs.pmode=1;GSMSourceGSRegs.dispfb1=(u64)10<<9;
 GSMSourceGSRegs.display1=(u64)639<<32;
 e->magic=RA_EVENT_MAGIC;e->seq=e->commit=1;e->kind=RA_EVENT_MAKE_UNLOCK(1);
 strcpy(e->title,"Get Off, You Psycho!");RA_OverlayOnVblank(100);
 assert(ra_ovl_seen==1 && !ra_ovl_running && !GIF_D2_CHCR && !flushed);
 e->seq=e->commit=2;e->kind=RA_EVENT_RESET;RA_OverlayOnVblank(101);
 RA_OverlayOnVblank(102);assert(resets==1 && !GIF_D2_CHCR);
 puts("PASS: recovery build consumes events without GS DMA; reset preserved");
}
'''
 features=(root/'opl/modules/network/common/ra_features.h').read_text()
 (p/'disabled.c').write_text(features+disabled)
 subprocess.run(['gcc','-std=gnu99','-Wno-pointer-to-int-cast',str(p/'disabled.c'),'-o',str(p/'disabled')],check=True)
 subprocess.run([str(p/'disabled')],check=True)
 static=code.split('int main(void){')[0]+r'''
int main(void){
 u64 packet[(RA_PACKET_BYTES+15)/8+8]={0};
 struct ra_event *e=RA_OverlayEventBuffer();RA_OverlaySetPacketBuffer(packet);
 assert(ra_ovl_running && ra_ovl_points==1 && !strcmp(ra_ovl_title,"Get Off, You Psycho!"));
 RA_OverlayOnVblank(1);assert(!GIF_D2_CHCR);
 GSMSourceGSRegs.pmode=1;GSMSourceGSRegs.dispfb1=(u64)10<<9;
 GSMSourceGSRegs.display1=(u64)639<<32;
 RA_OverlayOnVblank(2);assert(ra_dirty&&!GIF_D2_CHCR);
 finish_card(2);validate_packet(packet);
 assert(RA_CARD_W==192 && RA_CARD_H==40 && GIF_D2_QWC*16==30816);
 u32 *pixels=(u32*)((char*)packet+96);
 unsigned int x=(packet[4]>>32)&0x7ff,y=(packet[4]>>48)&0x7ff;
 for(int row=0;row<RA_CARD_H;row++)for(int col=0;col<RA_CARD_W;){
  int end=col+1;while(end<RA_CARD_W&&pixels[row*RA_CARD_W+end]==pixels[row*RA_CARD_W+col])end++;
  printf("RECT %u %u %u %u %06x\n",x+col,y+row,x+end,y+row+1,pixels[row*RA_CARD_W+col]&0xffffff);col=end;
 }
 for(unsigned int frame=1000;frame<10000;frame+=1000){
  flushed=0;GIF_D2_CHCR=0;RA_OverlayOnVblank(frame);assert(ra_ovl_running);validate_packet(packet);
  assert(flushed==0); /* stable framebuffer: reuse packet without cache operations */
 }
 e->magic=RA_EVENT_MAGIC;e->seq=e->commit=1;e->kind=RA_EVENT_MAKE_UNLOCK(9);
 strcpy(e->title,"Server title");GIF_D2_CHCR=0;RA_OverlayOnVblank(10001);
 assert(ra_ovl_points==1 && !strcmp(ra_ovl_title,"Get Off, You Psycho!"));
 e->seq=e->commit=2;e->kind=RA_EVENT_RESET;RA_OverlayOnVblank(10002);assert(resets==1);
 RA_OverlaySetPacketBuffer(NULL);GIF_D2_CHCR=0;RA_OverlayOnVblank(10003);
 assert(!GIF_D2_CHCR && !ra_ovl_running);
 puts("PASS: static card starts without unlocks, never expires, keeps fixed text, waits for display, preserves reset");
}
'''
 (p/'static.c').write_text(static)
 subprocess.run(['gcc','-DRA_STATIC_CARD_TEST=1','-std=gnu99','-Wno-pointer-to-int-cast',str(p/'static.c'),'-o',str(p/'static')],check=True)
 output=subprocess.check_output([str(p/'static')],text=True)
 if '--preview-static' in sys.argv:(root/'pcsx2-test/ra-static-card-primitives.txt').write_text(output)
 print(output.splitlines()[-1])
