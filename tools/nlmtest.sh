#!/bin/bash
# NekoLM C engine vs the Python integer reference (exact logits), then generation speed on this host.
#   nlmtest.sh [LINES] [sweep]   sweep = pass rate of the on-device checks per sampling setting
set -euo pipefail
cd /mnt/c/Users/tyler/M15-Backup/neko-arcade
mkdir -p build
gcc -O2 -Wall -Isrc -o build/nlm_test tools/nlm_test.c src/nekolm.c src/gen/nekolm_data.c src/lines.c src/gen/lines_data.c
./build/nlm_test build/nekolm_vectors.txt "${1:-60}" ${2:-}
