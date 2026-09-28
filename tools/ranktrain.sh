#!/bin/bash
# Fine-tune NekoLM as the composer ranker: ranktrain.sh LOGNAME ARGS...  (env LABELLERS, RANK_OUT pass through)
cd /mnt/c/Users/tyler/M15-Backup/neko-arcade
log=/root/rank-train-$1.log; shift
exec /root/neko-venv/bin/python -u tools/rank_train.py "$@" 2>&1 | grep --line-buffered -v -i -e numpy -e conversion_method > $log
