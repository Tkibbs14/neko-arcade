#!/bin/bash
# Run a command detached from this WSL call (survives wsl.exe exiting): bg.sh LOGFILE command args...
log=$1; shift
cd /mnt/c/Users/tyler/M15-Backup/neko-arcade
setsid nohup "$@" > "$log" 2>&1 < /dev/null &
echo "started pid $! -> $log"
