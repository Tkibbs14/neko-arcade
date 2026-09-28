#!/bin/bash
# Builds Neko Arcade three ways from one source tree (run inside WSL):
#   build/native/nekoarcade_libretro.so  x86-64 core for headless tests (lrtest)
#   build/stick/nekoarcade_libretro.so   MIPS32r2 core for the M15 stick (picoarch)
#   build/web/nekoarcade.wasm + neko-arcade.html   browser build for the PC
# Usage: build.sh [native|stick|web|all]   (default all)
set -euo pipefail
P=/mnt/c/Users/tyler/M15-Backup/neko-arcade
W=/root/m15; S=$W/sysroot; L=$S/usr/lib/mipsel-linux-gnu
cd "$P"
[ -f third_party/libretro.h ] || { mkdir -p third_party; cp $W/TreeFrogUI_picoarch/libretro-common/include/libretro.h third_party/; }
python3 tools/paint_bust.py
python3 tools/artgen.py
GAME=""
for f in src/*.c src/gen/*.c; do
    case "$f" in src/platform_*) ;; *) GAME="$GAME $f" ;; esac
done
# the model engine needs the exported weights (tools/nekolm.py export)
[ -f src/gen/nekolm_data.h ] || GAME="${GAME/ src\/nekolm.c/}"
WARN="-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers"
what=${1:-all}

if [ "$what" = native ] || [ "$what" = all ]; then
    mkdir -p build/native
    gcc -O2 -g -fPIC -shared $WARN -Isrc -Ithird_party -o build/native/nekoarcade_libretro.so $GAME src/platform_libretro.c
    echo "native: $(stat -c %s build/native/nekoarcade_libretro.so) bytes"
fi

if [ "$what" = stick ] || [ "$what" = all ]; then
    mkdir -p build/stick
    GCCINC=$(mipsel-linux-gnu-gcc -print-file-name=include)
    mipsel-linux-gnu-gcc -O2 -fPIC -shared $WARN -U_FILE_OFFSET_BITS -U_TIME_BITS -march=mips32r2 -mtune=74kc \
        -mfp32 -mhard-float -EL --sysroot=$S -nostdinc -isystem $GCCINC -isystem $S/usr/include/mipsel-linux-gnu \
        -isystem $S/usr/include -Isrc -Ithird_party -Wl,--hash-style=sysv -Wl,-z,defs -nostdlib \
        -o build/stick/nekoarcade_libretro.so $GAME src/platform_libretro.c -lgcc -L$L -lc
    mipsel-linux-gnu-strip build/stick/nekoarcade_libretro.so
    echo "stick: $(stat -c %s build/stick/nekoarcade_libretro.so) bytes, needs: $(mipsel-linux-gnu-objdump -T build/stick/nekoarcade_libretro.so | grep -o 'GLIBC_[0-9.]*' | sort -uV | tr '\n' ' ')"
fi

if [ "$what" = web ] || [ "$what" = all ]; then
    mkdir -p build/web
    clang --target=wasm32 -O2 -nostdlib -ffreestanding -fno-builtin $WARN -Isrc -Ithird_party \
        -Wl,--no-entry -Wl,--export-memory -Wl,-z,stack-size=262144 \
        -o build/web/nekoarcade.wasm $GAME src/platform_web.c
    python3 tools/mkweb.py
    echo "web: $(stat -c %s build/web/nekoarcade.wasm) bytes wasm"
fi
