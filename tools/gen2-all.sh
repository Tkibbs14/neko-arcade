#!/bin/bash
# The second generation of teacher lines, both rounds (network-bound): gen2-all.sh  (log: /root/gen2-all.log)
cd /mnt/c/Users/tyler/M15-Backup/neko-arcade
{ /root/neko-venv/bin/python -u tools/distill_gen2.py deepseek/deepseek-v4.1-flash content/distill/v2 60 1 && /root/neko-venv/bin/python -u tools/distill_gen2.py deepseek/deepseek-v4.1-flash content/distill/v2 60 2; } 2>&1 | grep --line-buffered -v -i -e numpy -e conversion > /root/gen2-all.log
