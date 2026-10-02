"""Validate the OPL CADR2 parser against real-format replies and network failures."""
from pathlib import Path
import subprocess
import tempfile

source=(Path(__file__).resolve().parents[2]/'opl/src/ranet.c').read_text()
function=source[source.index('int raAskCaduceus('):source.index('int raCaduceusPage(')]
code=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
typedef unsigned char u8;
typedef unsigned int u32;
static char g_rx[2048],response[256];
static u8 pc_ip[4]={192,168,1,81};
static int available=1,replies=1,calls,closed;
static int open_pc_socket(char *a,int n,u8 *ip){return available?3:-1;}
static void broadcast_target(struct sockaddr_in *to){memset(to,0,sizeof(*to));}
static void disconnect(int s){closed++;}
static int ask(int s,struct sockaddr_in *to,const char *req,char *out,int size){
 assert(!strncmp(req,"CADQ2 ",6));calls++;if(!replies)return -1;
 snprintf(out,size,"%s",response);return strlen(out);
}
'''+function+r'''
int main(void){
 const char *hash="0123456789abcdef0123456789abcdef";char title[96],detail[96];int ready;
 snprintf(response,sizeof(response),"CADR2 %s READY OK 120 Bully",hash);
 assert(raAskCaduceus(hash,title,96,detail,96,&ready)==0&&ready==1);
 assert(!strcmp(title,"Bully")&&strstr(detail,"conectada"));
 snprintf(response,sizeof(response),"CADR2 %s OFFLINE OK 120 Bully",hash);
 assert(raAskCaduceus(hash,title,96,detail,96,&ready)==0&&ready==0);
 assert(strstr(detail,"sem conquistas"));
 snprintf(response,sizeof(response),"CADR2 %s READY NO",hash);
 assert(raAskCaduceus(hash,title,96,detail,96,&ready)==1);
 snprintf(response,sizeof(response),"CADR2 %s OFFLINE UNKNOWN",hash);
 assert(raAskCaduceus(hash,title,96,detail,96,&ready)==-7);
 strcpy(response,"CADR2 wrong-hash READY OK 120 Bully");
 assert(raAskCaduceus(hash,title,96,detail,96,&ready)==-3&&ready==0);
 snprintf(response,sizeof(response),"CADR2 %s READY OK 0 Bully",hash);
 assert(raAskCaduceus(hash,title,96,detail,96,&ready)==-3);
 replies=0;calls=0;assert(raAskCaduceus(hash,title,96,detail,96,&ready)==-2&&calls==2&&ready==0);
 available=0;calls=0;assert(raAskCaduceus(hash,title,96,detail,96,&ready)==-1&&!calls&&ready==0);
 assert(closed==7);return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='caduceus-reply-') as directory:
    codepath=Path(directory)/'test.c';codepath.write_text(code)
    executable=Path(directory)/'test'
    subprocess.run(['gcc','-std=c99','-Wall','-Werror',str(codepath),'-o',str(executable)],check=True)
    subprocess.run([str(executable)],check=True)
print('PASS: CADR2 session, unsupported, unknown, wrong hash, malformed count, no server and no network')
