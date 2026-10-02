"""Exercise the real card input handler with pad/launch stubs on the host."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'opl/src/menusys.c').read_text()
start = source.index('void menuHandleInputGameCard(void)')
handler = source[start:source.index('static void menuOpenAchievements(', start)]
test = r'''
#include <assert.h>
#include <stddef.h>
enum { KEY_CROSS=1,KEY_CIRCLE,KEY_LEFT,KEY_RIGHT,KEY_SELECT,KEY_L1,KEY_SQUARE,
       GUI_SCREEN_MAIN, SFX_CURSOR, SB_RA_SUPPORTED, SB_RA_ERROR, SB_RA_UNSUPPORTED };
typedef struct {const char *name,*extension,*startup;int format;} base_game_info_t;
typedef struct item_list { const char *(*itemGetPrefix)(struct item_list *); } item_list_t;
typedef struct {int state,session_ready;char hash[33];} sb_ra_check_result_t;
typedef struct item {void *current;item_list_t *userdata;void (*execSquare)(struct item *);
 void (*execCircle)(struct item *);void (*execCross)(struct item *);} item_t;
typedef struct {item_t *item;} menu_t;
static menu_t *selected_item;
static int key,gSelectButton=KEY_CROSS,cardAction,cardShowHash,cardNeedsCheck,busy,screen,launches,enabled=-1,checks;
static sb_ra_check_result_t result;
static base_game_info_t game={"game",".iso","SLUS",2};
static int getKeyOn(int k){return key==k;}
static void guiSwitchScreen(int s){screen=s;}
static void sfxPlay(int s){}
static void menuCheckSelectedRA(void){checks++;}
static int sbGameCheckBusy(void){return busy;}
static base_game_info_t *menuCardImage(item_list_t *s){return &game;}
static int sbGetGameCheck(const char *p,const char *n,const char *e,const char *s,int f,sb_ra_check_result_t *r){*r=result;return 1;}
static void sbSetRALaunchEnabled(int n){enabled=n;}
static const char *prefix(item_list_t *s){return "smb0:";}
static void launch(item_t *s){launches++;}
static int achievementOpens;
static void menuOpenAchievements(const char *hash){achievementOpens++;}
'''
test += handler
test += r'''
int main(void){
 item_list_t support={prefix};item_t item={&game,&support,launch,launch,launch};menu_t menu={&item};selected_item=&menu;
 cardNeedsCheck=1;key=KEY_CROSS;menuHandleInputGameCard();assert(!launches);
 cardNeedsCheck=0;busy=1;menuHandleInputGameCard();assert(!launches);
 key=KEY_RIGHT;menuHandleInputGameCard();assert(cardAction==2);
 menuHandleInputGameCard();assert(cardAction==1);
 key=KEY_CROSS;menuHandleInputGameCard();assert(screen==GUI_SCREEN_MAIN&&!launches);
 key=KEY_LEFT;menuHandleInputGameCard();assert(cardAction==2);
 menuHandleInputGameCard();assert(cardAction==0);
 busy=0;result.state=SB_RA_ERROR;key=KEY_CROSS;menuHandleInputGameCard();assert(launches==1&&enabled==0);
 result.state=SB_RA_SUPPORTED;result.session_ready=0;menuHandleInputGameCard();assert(launches==2&&enabled==0);
 result.session_ready=1;menuHandleInputGameCard();assert(launches==3&&enabled==1);
 cardAction=2;menuHandleInputGameCard();assert(achievementOpens==1&&launches==3);cardAction=0;
 result.state=SB_RA_UNSUPPORTED;menuHandleInputGameCard();assert(launches==4&&enabled==0);
 gSelectButton=KEY_CIRCLE;key=KEY_CROSS;screen=0;menuHandleInputGameCard();assert(screen==GUI_SCREEN_MAIN&&launches==4);
 key=KEY_CIRCLE;menuHandleInputGameCard();assert(launches==5);
 selected_item=NULL;menuHandleInputGameCard();assert(screen==GUI_SCREEN_MAIN);
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='opl-card-actions-') as directory:
    code=Path(directory)/'test.c'
    code.write_text(test)
    executable=Path(directory)/'test'
    subprocess.run(['gcc','-std=c99','-Wall','-Werror',str(code),'-o',str(executable)],check=True)
    subprocess.run([str(executable)],check=True)
print('PASS: pending blocks play, back remains available, offline/unsupported play, authenticated RA, swapped confirm')
