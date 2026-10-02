"""Exercise the bounded on-console page parser with malformed and partial packets."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'opl/src/achievements.c').read_text()
types=(root/'opl/include/achievements.h').read_text()
parser=source[source.index('static char *field('):source.index('static void loadPage(')]
code='#include <assert.h>\n#include <stdio.h>\n#include <string.h>\n#include <stdlib.h>\n'+types+parser+r'''
int main(void){
 achievement_page_t result;
 char good[]="OK\tA\t0\t2\t3057\t1\t2\tPlayer\tBully\n1\t0\t1\t0\t5\t0123456789abcdef0123456789abcdef\tWelcome\tStart the game\t2026-01-01\n2\t0\t0\t0\t10\t-\tWinner\tFinish the game\t";
 assert(achievementsParse(good,&result)&&result.state==ACH_READY&&result.count==2);
 assert(result.entries[0].earned&&result.entries[1].date[0]==0&&result.entries[1].icon[0]==0);
 char offline[]="OFFLINE";assert(achievementsParse(offline,&result)&&result.state==ACH_OFFLINE);
 char unsupported[]="UNSUPPORTED";assert(achievementsParse(unsupported,&result)&&result.state==ACH_UNSUPPORTED);
 char empty[]="OK\tG\t0\t0\t0\t0\t0\tPlayer\tLibrary\n";
 assert(achievementsParse(empty,&result)&&result.count==0);
 char bad[]="OK\tA\t-1\t2\t3057\t1\t2\tPlayer\tBully";assert(!achievementsParse(bad,&result));
 char partial[]="OK\tA\t0";assert(!achievementsParse(partial,&result));
 char invalid[]="OK\tA\t0\t1\t1\t0\t1\tP\tG\n1\t0\t0\t0\t0\t../../secret\tt\td\t";
 assert(!achievementsParse(invalid,&result));
 char none[]="";assert(!achievementsParse(none,&result));
 return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
 path=Path(directory);(path/'test.c').write_text(code)
 subprocess.run(['gcc','-std=c99','-Wall','-Werror','-fsanitize=address,undefined',str(path/'test.c'),'-o',str(path/'test')],check=True)
 subprocess.run([str(path/'test')],check=True)
print('PASS: bounded pages, account offline, empty filters, truncated packets, invalid numbers and icon traversal')
