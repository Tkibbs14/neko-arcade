#!/bin/bash
# nlm_fill (C) vs c_fill (Python) on every composable line: filltest.sh
set -euo pipefail
cd /mnt/c/Users/tyler/M15-Backup/neko-arcade
gcc -O2 -w -Isrc -o build/fill_test tools/fill_test.c src/nekolm.c src/gen/nekolm_data.c
python3 tools/filltest.py
