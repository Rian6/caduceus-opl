"""Regression: fitting a label must not scan past its terminating NUL."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "opl/src/fntsys.c").read_text()
function = source[source.index("void fntFitString("):source.index("int fntCalcDimensions(", source.index("void fntFitString("))]
test = r'''
#include <assert.h>
#include <string.h>
#include <stddef.h>
static size_t rmScaleX(size_t x) { return x; }
static int fntCalcDimensions(int id, const char *s) { return strlen(s); }
'''
test += function
test += r'''
int main(void) {
    char shortLabel[] = "Game ID";
    char longWord[] = "0123456789abcdef";
    char wrapped[] = "One two three";
    char empty[] = "";
    fntFitString(0, shortLabel, 20);
    assert(!strcmp(shortLabel, "Game ID"));
    fntFitString(0, longWord, 4);
    assert(!strcmp(longWord, "0123456789abcdef"));
    fntFitString(0, wrapped, 7);
    assert(!strcmp(wrapped, "One two\nthree"));
    fntFitString(0, empty, 7);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="opl-font-wrap-") as directory:
    path = Path(directory)
    (path / "test.c").write_text(test)
    subprocess.run(["gcc", "-fsanitize=address", "-g", str(path / "test.c"), "-o", str(path / "test")], check=True)
    subprocess.run([str(path / "test")], check=True)
print("PASS: text terminators, long word, wrapping and empty label (AddressSanitizer)")
