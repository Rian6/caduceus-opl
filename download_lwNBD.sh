#!/bin/bash

## Download lwNBD
REPO_URL="https://github.com/bignaux/lwNBD.git"
REPO_FOLDER="modules/network/lwNBD"
COMMIT="15f1c14536d662e2e00d38d0aa91ef24149e8b96"
if test ! -d "$REPO_FOLDER"; then
  git clone $REPO_URL "$REPO_FOLDER" || { exit 1; }
  (cd $REPO_FOLDER && git checkout "$COMMIT" && cd -) || { exit 1; }
else
  (cd "$REPO_FOLDER" && git fetch origin && git checkout "$COMMIT" && cd - )|| exit 1
fi

# The IOP build uses PS2SDK/PS2DEV from the calling environment.
# Upstream requires this file even when no local overrides are needed.
if test ! -f "$REPO_FOLDER/.env"; then
  printf "%s\n" "# No local overrides; SDK paths are supplied by the build environment." > "$REPO_FOLDER/.env"
fi
