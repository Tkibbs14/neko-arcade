#!/bin/bash
# Release build: all three targets, the model parity check, and the link-preview image (Nia's desk).
set -euo pipefail
cd /mnt/c/Users/tyler/M15-Backup/neko-arcade
bash tools/build.sh all 2>&1 | grep -v '^Wrote\|preview\|paint_bust\|artgen' | tail -4
bash tools/nlmtest.sh 6 | head -1
bash tools/shot.sh prev 150 '' 150 | tail -1
python3 tools/zoom.py build/shots/prev_150.png build/web/preview.png 3
sha256sum build/stick/nekoarcade_libretro.so build/web/neko-arcade.html build/web/preview.png
