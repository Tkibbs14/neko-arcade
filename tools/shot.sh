#!/bin/bash
# Headless screenshots of the native build: shot.sh TAG FRAMES "INPUT" [DUMP_AT]
#   INPUT uses lrtest syntax "from-to:button,..." (B=0 Y=1 SELECT=2 START=3 UP=4 DOWN=5 LEFT=6 RIGHT=7 A=8 X=9 L=10 R=11)
# PNGs land in build/shots/TAG_<frame>.png
set -euo pipefail
P=/mnt/c/Users/tyler/M15-Backup/neko-arcade
LR=/root/m15/out/lrtest_native
if [ ! -x $LR ] || [ /mnt/c/Users/tyler/M15-Backup/m15-mod/src/lrtest.c -nt $LR ]; then
    gcc -O2 -o $LR /mnt/c/Users/tyler/M15-Backup/m15-mod/src/lrtest.c -I$P/third_party -ldl
fi
mkdir -p $P/build/shots
cd /tmp
NEKO_LOG=- LRTEST_DUMP=$P/build/shots/$1 LRTEST_DUMP_AT=${4:-} LRTEST_INPUT="${3:-}" \
    $LR $P/build/native/nekoarcade_libretro.so /tmp/none.neko "$2" 2>&1 | grep -E 'RESULT|saved|rror|nekoarcade' | tail -${SHOT_LINES:-12}
