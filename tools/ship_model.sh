#!/bin/bash
# Put a NekoLM float checkpoint into the game: ship_model.sh CKPT
#   The first run keeps the original model as build/nekolm_base.pt (rank_train.py always starts from it).
#   Quantizes, exports src/gen/nekolm_data.c, checks the C engine against the Python integer reference
#   (must say "exact match"), then reruns the game's composer -> build/quality/composer.tsv for the panel.
set -euo pipefail
cd /mnt/c/Users/tyler/M15-Backup/neko-arcade
PY=/root/neko-venv/bin/python
[ -f build/nekolm_base.pt ] || cp build/nekolm.pt build/nekolm_base.pt
[ "$1" -ef build/nekolm.pt ] || cp "$1" build/nekolm.pt
$PY -u tools/nekolm.py quant 2>&1 | grep -v -i -e numpy -e conversion_method | tail -3
$PY -u tools/nekolm.py export 2>&1 | grep -v -i -e numpy -e conversion_method
bash tools/nlmtest.sh 20 | head -4
bash tools/composeeval.sh 2
