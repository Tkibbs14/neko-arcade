#!/bin/bash
# Round 2 of the teacher lines (network-bound), logged for a monitor.
cd /mnt/c/Users/tyler/M15-Backup/neko-arcade
exec python3 -u tools/distill_gen.py 60 2 > /root/distill-r2.log 2>&1
