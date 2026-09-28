#!/bin/bash
# Build and run the Rival! playtest simulator against the real game code: rivalsim.sh [matches]
set -euo pipefail
cd /mnt/c/Users/tyler/M15-Backup/neko-arcade
SRC=""
for f in src/*.c src/gen/*.c; do
    case "$f" in src/platform_*|src/demo_rival.c) ;; *) SRC="$SRC $f" ;; esac
done
gcc -O2 -w -Isrc -Ithird_party -o build/rival_sim tools/rival_sim.c $SRC
./build/rival_sim "${1:-40}"
