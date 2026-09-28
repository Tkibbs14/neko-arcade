#!/bin/bash
# Ranking labels for NekoLM: ranklabels.sh train|eval TAG MODEL [EFFORT] [MAX] [--voice]
# (log: /root/rank-labels-<all arguments>.log)
cd /mnt/c/Users/tyler/M15-Backup/neko-arcade
log="/root/rank-labels-$(echo "$*" | tr " /" "__").log"
exec /root/neko-venv/bin/python -u tools/rank_labels.py "$@" > "$log" 2>&1
