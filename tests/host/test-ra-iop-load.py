"""Run real RA startup allocation and module error handling without IOP hardware."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'opl/ee_core/src/iopmgr.c').read_text()
start=s.index('    RA_OverlaySetPacketBuffer(config->raOverlayBuf);')
end=s.index('\n}\n',start)
allocation=s[start:end]
s=(root/'opl/ee_core/src/modmgr.c').read_text()
start=s.index('int LoadOPLModule(')
end=s.index('\n/*---',start)
module=s[start:end]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t u32;
#include "ra_snap.h"
#define OPL_MODULE_ID_RAUDP 16
#define ETH_MODE 2
#define IPCONFIG_MAX_LEN 64
static int EnableDebug,allocation_size,allocations,loads,frees,fail_alloc,module_result=4;
static unsigned int ra_snap_iop;
static char g_ipconfig[64]="192.168.1.67";static int g_ipconfig_len=13;
struct config_t{void *raOverlayBuf;int raWatchCount,raSnapBytes,raNodeCount,GameMode;char GameID[16],raServerIP[16];};
static struct config_t settings;static struct config_t *config=&settings;
#define DBGCOL(...) ((void)0)
#define DBGCOL_BLNK(...) assert(0)
#define BGCOLND(...) ((void)0)
#define MODMGR 0
static void delay(int n){}
static int GetOPLModInfo(int id,void **p,unsigned int *n){*p=(void*)0x10000000;*n=16;return 0;}
static int LoadMemModule(int mode,void*p,unsigned int n,int len,const char*args){loads++;return module_result;}
static void *SifAllocIopHeap(int bytes){allocations++;allocation_size=bytes;return fail_alloc?NULL:(void*)0x10000000;}
static void SifFreeIopHeap(void *p){assert(p==(void*)0x10000000);frees++;}
static void RA_OverlaySetPacketBuffer(void*p){}
static void *RA_OverlayEventBuffer(void){return (void*)0x20000000;}
static void ra_hex32(unsigned int value,char *p){memset(p,'0',8);}
'''+module+'\nstatic void load_ra(void){\n'+allocation+r'''
}
int main(void){
 settings.raWatchCount=561;settings.raSnapBytes=697;settings.GameMode=ETH_MODE;
 strcpy(settings.GameID,"SLUS_212.69");strcpy(settings.raServerIP,"192.168.1.81");
 load_ra();assert(allocations==1&&loads==1&&allocation_size==RA_SNAP_TOTAL_FOR(697));
 assert(allocation_size<RA_SNAP_TOTAL&&ra_snap_iop);
 module_result=-400;load_ra();assert(!ra_snap_iop&&frees==1&&loads==2);
 fail_alloc=1;load_ra();assert(!ra_snap_iop&&loads==2&&frees==1);
 fail_alloc=0;settings.raSnapBytes=RA_SNAP_MAX_BYTES;settings.raNodeCount=1;
 int before=allocations;load_ra();assert(allocations==before&&!ra_snap_iop);
 settings.raNodeCount=-1;load_ra();assert(allocations==before&&!ra_snap_iop);
 settings.raNodeCount=3;settings.raSnapBytes=697;module_result=4;
 load_ra();assert(allocation_size==RA_SNAP_TOTAL_FOR(697+3*RA_NODE_PAIR_BYTES));
 printf("PASS: game-sized IOP snapshot (%d bytes for Bully); optional module OOM returns; failed allocation skips RA; chain bounds\n",RA_SNAP_TOTAL_FOR(697));
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['gcc','-std=gnu99','-Wno-pointer-to-int-cast','-I',str(root/'opl/modules/network/common'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,timeout=3)
