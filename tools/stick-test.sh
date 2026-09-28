#!/bin/bash
# Run the stick build of Neko Arcade under qemu-mipsel against the stick's own firmware libraries (the
# extracted root in /root/m15/stickroot): proves the core loads and runs with the libc the M15 really has.
#   stick-test.sh FRAMES "INPUT" [DUMP_AT]   (lrtest input syntax; PNGs in build/shots/stick_<frame>.png)
set -euo pipefail
W=/root/m15; R=$W/stickroot; P=/mnt/c/Users/tyler/M15-Backup/neko-arcade
cd /tmp
rm -f /tmp/nekoarcade.log
start=$(date +%s%N)
LRTEST_DUMP=$P/build/shots/stick LRTEST_DUMP_AT=${3:-} LRTEST_INPUT="${2:-}" \
    qemu-mipsel -cpu 74Kf -L "$R" -E LD_LIBRARY_PATH=/lib:/usr/lib "$W/out/lrtest" \
    $P/build/stick/nekoarcade_libretro.so /tmp/none.neko "${1:-300}" 2>&1 | grep -E 'RESULT|saved|rror|undefined|nekoarcade' | tail -16
echo "qemu wall time: $(( ($(date +%s%N) - start) / 1000000 )) ms for ${1:-300} frames (emulated; not the real chip speed)"
echo "--- playtest log (written by the stick build):"; cat /tmp/nekoarcade.log 2>/dev/null | tail -${LOG_LINES:-20}
