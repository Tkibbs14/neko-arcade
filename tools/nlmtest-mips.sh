#!/bin/bash
# The model parity check (every dot-product kernel, including the inline-asm madd) as a MIPS 74K binary under
# qemu: the stick's code path must match the Python integer reference exactly too.
set -euo pipefail
cd /mnt/c/Users/tyler/M15-Backup/neko-arcade
mipsel-linux-gnu-gcc -O2 -march=mips32r2 -mtune=74kc -static -Isrc -o build/nlm_test_mips tools/nlm_test.c \
    src/nekolm.c src/gen/nekolm_data.c src/lines.c src/gen/lines_data.c
qemu-mipsel -cpu 74Kf build/nlm_test_mips build/nekolm_vectors.txt "${1:-3}" | head -${2:-4}
