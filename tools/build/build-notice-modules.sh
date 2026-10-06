#!/usr/bin/env bash
set -euo pipefail
export PS2DEV="${PS2DEV:-/usr/local/ps2dev}"
export PS2SDK="$PS2DEV/ps2sdk"
export PATH="$PS2DEV/bin:$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PS2SDK/bin:$PATH"
cd "$(dirname "$0")/../../opl"
make -C modules/network/smap-ingame -j1
make -C modules/network/raudp -j1
