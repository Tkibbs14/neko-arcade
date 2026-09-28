#!/bin/bash
# NekoLM quality check, step 1: sample the evaluation set with the exact on-device engine.
#   quality.sh [PASS_TARGET] [MAX_TRIES] [TEMP_Q8 TOPK]   -> build/quality/student.tsv
set -euo pipefail
cd /mnt/c/Users/tyler/M15-Backup/neko-arcade
mkdir -p build/quality
gcc -O2 -Wall -Isrc -o build/nlm_eval tools/nlm_eval.c src/nekolm.c src/gen/nekolm_data.c src/lines.c src/gen/lines_data.c
./build/nlm_eval "${1:-3}" "${2:-12}" ${3:-} ${4:-} > build/quality/student.tsv
echo "student.tsv: $(wc -l < build/quality/student.tsv) attempts, $(awk -F'\t' '$4==1' build/quality/student.tsv | wc -l) passing"
