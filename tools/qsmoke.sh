#!/bin/bash
# Smoke test: judge only the first batch with every judge, then summarise.
cd /mnt/c/Users/tyler/M15-Backup/neko-arcade
QLIMIT=1 /root/neko-venv/bin/python -u tools/nlm_quality.py judge r1 2>&1 | grep -v -i numpy | grep -v conversion_method
python3 tools/qpeek.py r1
