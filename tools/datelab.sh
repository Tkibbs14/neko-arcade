#!/bin/bash
# Render the Thin Walls screens contact sheet: datelab.sh  -> build/date_lab.png
set -euo pipefail
cd /mnt/c/Users/tyler/M15-Backup/neko-arcade
gcc -O2 -w -Isrc -o build/date_lab tools/date_lab.c src/engine.c src/cast.c src/portrait.c src/save.c src/gen/art_data.c src/gen/date_data.c
./build/date_lab
python3 - <<'PY'
import sys
sys.path.insert(0, "tools")
data = open("build/date_lab.ppm", "rb").read()
head, rest = data.split(b"\n", 1)
_, w, h, _ = head.split()
w, h = int(w), int(h)
rows = [[tuple(rest[(y * w + x) * 3:(y * w + x) * 3 + 3]) for x in range(w)] for y in range(h)]
import importlib.util
spec = importlib.util.spec_from_file_location("zoom_mod", "tools/zoom.py")
src = open("tools/zoom.py").read().split("src, dst, s = ")[0]
ns = {}
exec(src, ns)
ns["write_png"]("build/date_lab.png", w, h, rows)
print("datelab: build/date_lab.png", w, "x", h)
PY
