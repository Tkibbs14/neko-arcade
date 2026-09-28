#!/bin/bash
cd /mnt/c/Users/tyler/M15-Backup/neko-arcade/tools
python3 -c "import distill_spec as d; print(len(d.SITS), len(d.VOICES))"
python3 -m py_compile distill_gen.py && echo distill_gen compiles
