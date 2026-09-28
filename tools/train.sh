#!/bin/bash
# Train NekoLM (run detached from Windows with Start-Process so it outlives any single tool call).
cd /mnt/c/Users/tyler/M15-Backup/neko-arcade
exec /root/neko-venv/bin/python -u tools/nekolm.py train "${1:-3000}" > /root/nekolm-train.log 2>&1
