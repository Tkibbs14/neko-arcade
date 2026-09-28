#!/bin/bash
# One headless screenshot per demo (native build), using input scripts checked by hand; PNGs are upscaled x3
# into build/shots/z_show_*.png.
set -euo pipefail
cd /mnt/c/Users/tyler/M15-Backup/neko-arcade
rm -f build/shots/show* build/shots/z_show*
run() {   # name frames input dump_at
    bash tools/shot.sh "show_$1" "$2" "$3" "$4" | grep -E 'nekoarcade|RESULT' || true
}
run hub 200 '' 150
run rival 700 '60-61:8,120-121:8,200-260:4,300-340:5,400-460:7' 520
run detective 1900 "$(cat build/det_long_input.txt)" 450,1500
run cafe 820 '30-31:5,50-51:5,80-81:8,130-131:8,220-221:8,300-301:8,380-381:8,460-461:8,570-571:7,590-591:7,615-616:8,735-736:8,745-746:8' 560,760
run kittens 900 '30-31:5,45-46:5,60-61:5,90-91:8,200-201:8,240-241:8,280-281:8,320-321:8,360-361:8,400-401:8,440-441:8' 620,860
run village 800 '30-31:5,45-46:5,60-61:5,75-76:5,100-101:8,180-181:8,230-231:8,250-320:7,330-331:8,420-421:8,500-501:8,560-561:8' 340,540
bash tools/zoomall.sh show 3 > /dev/null
ls build/shots/z_show*
