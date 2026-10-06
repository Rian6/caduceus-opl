#!/usr/bin/env bash
set -euo pipefail
export PS2DEV="${PS2DEV:-/usr/local/ps2dev}"
export PS2SDK="$PS2DEV/ps2sdk"
export GSKIT="$PS2DEV/gsKit"
export PATH="$PS2DEV/bin:$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PS2DEV/dvp/bin:$PS2SDK/bin:$PATH"
cd "$(dirname "$0")/../../opl"
static_card=0
achievement_card=0
case "${1:-}" in
    "") ;;
    --static-card) static_card=1 ;;
    --achievements) achievement_card=1 ;;
    *) echo "Usage: $0 [--static-card|--achievements]" >&2; exit 2 ;;
esac
build_flags=(PADEMU=1 RA_STATIC_CARD_TEST="$static_card" RA_ACHIEVEMENT_CARD="$achievement_card")
# Rebuild interface objects when changing feature flags, as make does not track them.
force_sources=()
interface_objects=()
for source in src/*.c; do
    force_sources+=(-W "$source")
    object="obj/$(basename "${source%.c}").o"
    if [[ -f "$object" ]]; then interface_objects+=("$object"); fi
done
# -W is not propagated to the recursive make used by `all`. Rebuild the
# existing interface objects here so PADEMU flags agree at link time.
make "${build_flags[@]}" -j4 "${force_sources[@]}" "${interface_objects[@]}" > xmb-build.log 2>&1
# Core flags must also agree when switching between recovery and static card.
core_sources=()
core_objects=()
for source in ee_core/src/*.{c,S}; do
    [[ -f "$source" ]] || continue
    core_sources+=(-W "src/$(basename "$source")")
    object="obj/$(basename "${source%.*}").o"
    if [[ -f "ee_core/$object" ]]; then core_objects+=("$object"); fi
done
make -C ee_core "${build_flags[@]}" -j4 "${core_sources[@]}" "${core_objects[@]}" >> xmb-build.log 2>&1
# Prepare the optional upstream environment file without machine-specific values.
if [[ -d modules/network/lwNBD && ! -f modules/network/lwNBD/.env ]]; then
    printf "%s\n" "# SDK paths are supplied by the build environment." > modules/network/lwNBD/.env
fi
# Finish IRX generation before the parallel build embeds their byte arrays.
make "${build_flags[@]}" -j1 modules/network/smap-ingame/smap.irx modules/network/raudp/raudp.irx >> xmb-build.log 2>&1
make "${build_flags[@]}" -j4 all >> xmb-build.log 2>&1
