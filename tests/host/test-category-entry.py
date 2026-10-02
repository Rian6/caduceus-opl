"""Host regression checks for deferred category activation and empty USB cleanup."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'opl/src/opl.c').read_text()
functions=source[source.index('static void itemInitSupport('):source.index('static void itemExecSelect(')]
code=r'''
#include <assert.h>
#include <stdlib.h>
enum {BDM_MODE=0,BDM_MODE4=4,APP_MODE=7,IO_MENU_UPDATE_DEFFERED=2,IO_CATEGORY_ENTER=5};
typedef struct item_list {short mode;int enabled;void *owner,*priv;void (*itemInit)(struct item_list*);} item_list_t;
typedef struct {item_list_t *support;} opl_io_module_t;
static opl_io_module_t list_support[8];
static int busy,fail,count,types[32];
static void *args[32];
static int sbGameCheckBusy(void){return busy;}
static int ioPutRequest(int type,void *arg){if(fail)return -1;types[count]=type;args[count++]=arg;return 0;}
static void moduleUpdateMenuInternal(opl_io_module_t *m,int a,int b){}
static void init(item_list_t *s){ioPutRequest(99,s);s->enabled=1;}
'''+functions
bdm=(root/'opl/src/bdmsupport.c').read_text()
code+=r'''
typedef struct {char bdmPrefix[32];void *bdmGames;int bdmULSizePrev,bdmGameCount;} bdm_device_data_t;
static int reads;
static void sbReadList(void **games,char *prefix,int *size,int *count){reads++;*count=2;}
'''+bdm[bdm.index('static int bdmUpdateGameList('):bdm.index('static int bdmGetGameCount(')]
code+=r'''
int main(void){
 item_list_t devices[8]={0};
 for(int i=0;i<8;i++){devices[i].mode=i;devices[i].itemInit=init;list_support[i].support=&devices[i];}
 moduleEnterCategory(&devices[7]);moduleEnterCategory(&devices[7]);assert(count==1);
 categoryEnterWorker(args[0]);assert(devices[7].enabled&&types[1]==99&&types[2]==IO_MENU_UPDATE_DEFFERED);
 count=0;moduleEnterCategory(&devices[0]);categoryEnterWorker(args[0]);assert(count==11);
 for(int i=0;i<5;i++)assert(devices[i].enabled&&types[1+i*2]==99&&types[2+i*2]==IO_MENU_UPDATE_DEFFERED);
 count=0;busy=1;moduleEnterCategory(&devices[6]);assert(!count);busy=0;
 fail=1;moduleEnterCategory(&devices[6]);assert(!categoryPending[6]);fail=0;
 bdm_device_data_t data={0};data.bdmGames=malloc(16);data.bdmGameCount=3;devices[0].priv=&data;
 assert(bdmUpdateGameList(&devices[0])==0&&!data.bdmGames&&!data.bdmGameCount&&!reads);
 data.bdmPrefix[0]='m';assert(bdmUpdateGameList(&devices[0])==2&&reads==1);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    path=Path(directory);(path/'test.c').write_text(code)
    subprocess.run(['gcc','-std=c99','-Wall','-Werror',str(path/'test.c'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
print('PASS: activation deduplication, module-before-scan ordering, all USB slots, RA busy, queue failure and empty USB cleanup')
