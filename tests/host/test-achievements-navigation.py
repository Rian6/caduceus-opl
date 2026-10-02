"""Exercise actual achievement navigation while pages are pending or empty."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'opl/src/menusys.c').read_text()
handler=source[source.index('void menuHandleInputAchievements(void)'):source.index('void menuRenderInfo(void)')]
code='#include <assert.h>\n#include <stdio.h>\n#include <string.h>\n'+(root/'opl/include/achievements.h').read_text()+r'''
enum {KEY_CIRCLE=1,KEY_CROSS,KEY_START,KEY_LEFT,KEY_RIGHT,KEY_SELECT,KEY_SQUARE,KEY_L1,KEY_R1,KEY_UP,KEY_DOWN,GUI_SCREEN_MENU,GUI_SCREEN_GAME_CARD,GUI_SCREEN_MAIN,SFX_CURSOR};
static int achFromCard,achNeedsPage,achPage,achFilter,achSelected,achLibraryPage;
static char achKind='G',achTarget[33]="0";
static int key,gSelectButton=KEY_CROSS,busy,screen;
static achievement_page_t snapshot;
void achievementsSnapshot(achievement_page_t *p){*p=snapshot;}
static int getKeyOn(int k){return k==key;}
static int getKey(int k){return k==key;}
static int sbGameCheckBusy(void){return busy;}
static void menuInitMainMenu(void){}
static void guiSwitchScreen(int s){screen=s;}
static void sfxPlay(int s){}
static void achievementReload(void){achNeedsPage=1;achSelected=0;}
'''+handler+r'''
int main(void){
 busy=1;achFromCard=1;key=KEY_CIRCLE;menuHandleInputAchievements();assert(screen==GUI_SCREEN_GAME_CARD);
 screen=0;key=KEY_R1;menuHandleInputAchievements();assert(achPage==0);
 achFromCard=0;achKind='G';key=KEY_CIRCLE;menuHandleInputAchievements();assert(screen==GUI_SCREEN_MAIN);
 busy=0;snapshot.state=ACH_READY;snapshot.total=8;snapshot.count=3;snapshot.entries[1].id=3057;
 key=KEY_DOWN;menuHandleInputAchievements();assert(achSelected==1);
 key=KEY_CROSS;menuHandleInputAchievements();assert(achKind=='A'&&!strcmp(achTarget,"3057")&&achNeedsPage);
 achNeedsPage=0;key=KEY_R1;menuHandleInputAchievements();assert(achPage==1&&achNeedsPage);
 achNeedsPage=0;key=KEY_SQUARE;menuHandleInputAchievements();assert(achFilter==1&&achPage==0&&achNeedsPage);
 achNeedsPage=0;snapshot.count=0;key=KEY_DOWN;menuHandleInputAchievements();assert(achSelected==0);
 busy=1;key=KEY_CIRCLE;menuHandleInputAchievements();assert(achKind=='G'&&!strcmp(achTarget,"0"));
 gSelectButton=KEY_CIRCLE;key=KEY_CROSS;menuHandleInputAchievements();assert(screen==GUI_SCREEN_MAIN);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
 path=Path(directory);(path/'test.c').write_text(code)
 subprocess.run(['gcc','-std=c99','-Wall','-Werror',str(path/'test.c'),'-o',str(path/'test')],check=True)
 subprocess.run([str(path/'test')],check=True)
print('PASS: back while loading, library exit, direct game selection, paging, filters, empty pages and swapped cancel')
