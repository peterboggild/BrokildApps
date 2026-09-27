#!/usr/bin/env bash
# Pull the panel decals from GitHub and rebuild with them compiled in.
#
# The CMake glob runs at CONFIGURE time, so a plain `cmake --build` after new
# PNGs arrive would quietly build without them — the reconfigure below is the
# whole point of this script existing.
set -u

REPO="c:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps"
SRC="C:/Users/peter/Dropbox/ACTIVITIES/00 VSCODE/BrokildApps/vst3-apps/full-metal-racket/plugin"
SET_DIR="$REPO/assets/fmr-panel-decals"

echo "== pulling BrokildApps =="
git -C "$REPO" pull --ff-only origin main 2>&1 | tail -2

if [ -d "$SET_DIR" ]; then
  n=$(ls -1 "$SET_DIR"/*.png 2>/dev/null | wc -l)
  echo "== $n decal(s) in assets/fmr-panel-decals =="
  ls -1 "$SET_DIR"/*.png 2>/dev/null | while read -r f; do
    printf '   %-24s %s\n' "$(basename "$f")" "$(du -h "$f" | cut -f1)"
  done
else
  echo "== assets/fmr-panel-decals does not exist yet =="
fi

echo "== slicing sheets into parts =="
powershell -NoProfile -File "$SRC/tools/ingest-decals.ps1"

echo "== reconfigure (re-runs the glob) =="
cmake -S "$SRC" -B "$SRC/build" 2>&1 | grep -i "panel decal" || echo "   (no decal line — check the CMake glob)"

echo "== build =="
cmake --build "$SRC/build" --config Release 2>&1 | grep -E " error " | head -10
echo "== done; install with tools/install.sh =="
