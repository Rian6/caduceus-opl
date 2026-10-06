"""Execute loader placement with fake RAM; card cannot overlap game memory."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'opl/src/rawatch.c').read_text()
source = source[source.index('static u32 *gBlockList'):source.index('u32 *GetWatchBlockList')]
code = (root / 'opl/modules/network/common/ra_features.h').read_text() + r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef uintptr_t u32; typedef unsigned char u8;
struct ra_node { unsigned int base, offset; };
static unsigned int gWatchList[1]; static struct ra_node gNodeList[1];
static int gWatchCount=1, gWatchBytes=4, gNodeCount;
#define RA_NODE_PAIR_BYTES 8
#define RA_SNAP_TOTAL_FOR(bytes) (((bytes)+64+63)&~63)
#define LOG(...) ((void)0)
#define raLaunchNote(...) ((void)0)
/* No physical RAM access: the real placement function's arithmetic is tested. */
#define memcpy(dst,src,size) ((void)0)
#define memset(dst,value,size) ((void)0)
''' + source + r'''
int main(void) {
 void *end=PlaceWatchBlock((void*)0x98000);
#if RA_ENABLE_EXPERIMENTAL_CARD
 assert(gBlockOverlay && (uintptr_t)end<=0x100000);
 assert((uintptr_t)end-(uintptr_t)gBlockOverlay==RA_OVERLAY_PACKET_BYTES);
 end=PlaceWatchBlock((void*)0xff000);
 assert(!gBlockOverlay && gBlockList && gBlockSnap);
 assert((uintptr_t)end<0x100000);
 end=PlaceWatchBlock((void*)0x1ffc000);
 assert(!gBlockOverlay && (uintptr_t)end<0x2000000);
#else
 assert(!gBlockOverlay && gBlockList && gBlockSnap);
 assert((uintptr_t)end-0x98000<256);
#endif
 gWatchCount=0;end=PlaceWatchBlock((void*)0x98000);
#if RA_STATIC_CARD_TEST
 assert(gBlockOverlay && end!=(void*)0x98000 && (uintptr_t)end<=0x100000);
#else
 assert(end==(void*)0x98000 && !gBlockOverlay && !gBlockList);
#endif
 puts("PASS: card memory bounds, low/relocated storage, telemetry fallback, no-list launch");
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / 'test.c').write_text(code)
    for flags in ([], ['-DRA_ENABLE_EXPERIMENTAL_CARD=1'], ['-DRA_STATIC_CARD_TEST=1']):
        subprocess.run(['gcc', *flags, '-std=gnu99', str(path / 'test.c'), '-o', str(path / 'test')], check=True)
        subprocess.run([str(path / 'test')], check=True)
