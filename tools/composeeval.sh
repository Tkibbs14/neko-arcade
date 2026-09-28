#!/bin/bash
# Run the game's composer for every situation: composeeval.sh [LINES_PER_SITUATION] -> build/quality/composer.tsv
set -euo pipefail
cd /mnt/c/Users/tyler/M15-Backup/neko-arcade
mkdir -p build/quality
gcc -O2 -w -Isrc -o build/compose_eval tools/compose_eval.c src/live.c src/nekolm.c src/gen/nekolm_data.c \
    src/gen/compose_data.c src/lines.c src/gen/lines_data.c
./build/compose_eval "${1:-2}" > build/quality/composer.tsv
wc -l < build/quality/composer.tsv
