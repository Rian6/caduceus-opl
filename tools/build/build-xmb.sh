#!/usr/bin/env bash
set -euo pipefail
export PS2DEV="${PS2DEV:-/home/rian/ps2dev}"
export PS2SDK="$PS2DEV/ps2sdk"
export GSKIT="$PS2DEV/gsKit"
export PATH="$PS2DEV/bin:$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PS2DEV/dvp/bin:$PS2SDK/bin:$PATH"
cd "$(dirname "$0")/../../opl"
# Rebuild interface objects when changing feature flags, as make does not track them.
force_sources=()
for source in src/*.c; do force_sources+=(-W "$source"); done
make PADEMU=1 -j4 "${force_sources[@]}" all > xmb-build.log 2>&1
