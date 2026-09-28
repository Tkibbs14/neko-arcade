#!/bin/bash
# Composer lab step with the project venv: lab.sh STEP TAG [args]
cd /mnt/c/Users/tyler/M15-Backup/neko-arcade
exec /root/neko-venv/bin/python -u tools/composer_lab.py "$@" 2>&1 | grep -v -i numpy | grep -v conversion_method
