#!/bin/bash
# Run a step of the NekoLM quality check with the project venv: q.sh STEP TAG
cd /mnt/c/Users/tyler/M15-Backup/neko-arcade
exec /root/neko-venv/bin/python -u tools/nlm_quality.py "$1" "$2" 2>&1 | grep -v -i numpy | grep -v conversion_method
