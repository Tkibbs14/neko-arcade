#!/bin/bash
# Long scripted detective run (input from build/det_long_input.txt).
cd /mnt/c/Users/tyler/M15-Backup/neko-arcade
bash tools/shot.sh detl 1900 "$(cat build/det_long_input.txt)" 400,600,1000,1330,1400,1440,1500,1560,1880
bash tools/zoomall.sh detl 3 > /dev/null
