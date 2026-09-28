#!/bin/bash
# Upscale build/shots/PREFIX_*.png by SCALE (default 3) into build/shots/z_*.png for review.
cd /mnt/c/Users/tyler/M15-Backup/neko-arcade/build/shots
for f in "$1"_*.png; do python3 ../../tools/zoom.py "$f" "z_$f" "${2:-3}"; done
ls z_"$1"_*.png
