"""A ready catalog must fetch a valid watch list before game telemetry starts."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'opl/src/supportbase.c').read_text()
start=s.index('static int sbPrepareRATelemetry'); end=s.index('void sbHashGame(const char *path, const char *name',start); end=s.index('\n}\n',end)+3; s=s[start:end]
code=r'''
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#define LOG(...) ((void)0)
#define GAME_FORMAT_USBLD 0
#define GAME_FORMAT_OLD_ISO 1
#define SB_RA_SUPPORTED 1
#define SB_RA_UNSUPPORTED 2
#define SB_RA_ERROR 3
#define SB_RA_FORMAT_UNSUPPORTED 4
static struct {int state,session_ready;char hash[33],title[96],detail[96];} ra_check_result;
static int catalog,authenticated=1;
static void raHashLogOpen(const char *p){}
static void raHashStep(const char *p){}
static void raHashSetStepLog(void *p){}
static void raHashLogAdd(const char *a,const char *b,const char *c){}
static void raHashLogClose(void){}
static void guiShowRANotice(const char *a,const char *b){}
static int raHashIsoDirect(const char *p,const char *s,char *hash){strcpy(hash,"bully-hash");return 0;}
static int raAskCaduceus(const char *hash,char *title,int n,char *detail,int m,int *ready){
 strcpy(title,"Bully");strcpy(detail,"120 conquistas");*ready=authenticated;return catalog;
}
static int count,requests,reply,valid;
static void ClearWatchList(void){count=0;}
static int GetWatchCount(void){return count;}
static int raAskPC(const char *hash,const char *serial,const char *path,char *info,int size,char *detail,int dsize){
 assert(!strcmp(hash,"bully-hash")&&!strcmp(serial,"SLUS_212.69")&&!strcmp(path,"smb0:"));
 assert(!info&&!detail);requests++;if(reply==0&&valid)count=720;return reply;
}
'''+s+r'''
int main(void){
 count=123;assert(!sbPrepareRATelemetry("bully-hash","SLUS_212.69","smb0:",0)&&count==0&&requests==0);
 reply=-2;assert(!sbPrepareRATelemetry("bully-hash","SLUS_212.69","smb0:",1)&&requests==1);
 reply=0;valid=0;assert(!sbPrepareRATelemetry("bully-hash","SLUS_212.69","smb0:",1));
 valid=1;assert(sbPrepareRATelemetry("bully-hash","SLUS_212.69","smb0:",1)&&count==720&&requests==3);
 requests=0;authenticated=0;sbHashGame("smb0:","Bully (USA)",".iso","SLUS_212.69",2);
 assert(requests==0&&!ra_check_result.session_ready&&ra_check_result.state==SB_RA_SUPPORTED);
 authenticated=1;reply=-2;sbHashGame("smb0:","Bully (USA)",".iso","SLUS_212.69",2);
 assert(requests==1&&!ra_check_result.session_ready&&strstr(ra_check_result.detail,"Lista RA"));
 reply=0;valid=1;sbHashGame("smb0:","Bully (USA)",".iso","SLUS_212.69",2);
 assert(requests==2&&ra_check_result.session_ready&&count==720&&!strcmp(ra_check_result.title,"Bully"));
 catalog=1;authenticated=0;sbHashGame("smb0:","Bully (USA)",".iso","SLUS_212.69",2);
 assert(requests==2&&ra_check_result.state==SB_RA_UNSUPPORTED);
 puts("PASS: complete game check and catalog readiness fetches watch list; offline, failed and empty lists cannot enable telemetry");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['gcc','-std=gnu99','-Wall','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
