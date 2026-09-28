#!/bin/bash
# Build and run tools/rank_eval.c (the device's composer scoring, 6 candidates): rankeval.sh < groups > scores
set -euo pipefail
cd /mnt/c/Users/tyler/M15-Backup/neko-arcade
gcc -O2 -w -DLIVE_CANDS=6 -Isrc -o build/rank_eval tools/rank_eval.c src/live.c src/nekolm.c src/gen/nekolm_data.c \
    src/gen/compose_data.c src/lines.c src/gen/lines_data.c
./build/rank_eval
