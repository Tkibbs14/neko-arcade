#!/usr/bin/env python3
"""Neko Arcade art pipeline.

Reads the hand-drawn text art in art/*.txt and writes:
  src/gen/art_data.h, src/gen/art_data.c   - sprite, palette and font data for the game
  build/previews/<file>.png                 - every sprite of a file at 4x, for checking by eye

Art file syntax (one file may hold any mix of blocks):
  palette NAME            fixed colours: one "c = #rrggbb" line per character, then "end"
  ramp NAME               recolourable slots: one "c = SLOT #rrggbb" line per character (SLOT 1..15,
                          colour = preview default), then "end"; sprites using it are drawn with a
                          16-colour ramp chosen at run time (hair, outfit, eyes ...)
  sprite NAME PALETTE     rows of characters ('.' = transparent) up to "end"
  font NAME               "glyph X" (X = the character, "space", or \\xNN) then 8 rows of '#'/'.';
                          the block ends with "end"
Lines starting with '#' between blocks are comments.
"""
import os, struct, sys, zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "art")
GEN = os.path.join(ROOT, "src", "gen")
PREV = os.path.join(ROOT, "build", "previews")


def die(msg):
    sys.exit("artgen: " + msg)


def parse_color(s):
    s = s.strip()
    if not (s.startswith("#") and len(s) == 7):
        die("bad colour " + s)
    return tuple(int(s[i:i + 2], 16) for i in (1, 3, 5))


def rgb565(c):
    r, g, b = c
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


palettes = {}   # name -> {char: (r,g,b)}
ramps = {}      # name -> {char: (slot, (r,g,b))}
sprites = []    # (name, palname, rows, source file)
fonts = {}      # name -> {code: [rows]}


def parse_file(path):
    lines = open(path, encoding="utf-8").read().split("\n")
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        i += 1
        if not line or line.startswith("#"):
            continue
        words = line.split()
        kind = words[0]
        if kind in ("palette", "ramp"):
            name = words[1]
            table = {}
            while i < len(lines):
                l = lines[i].strip()
                i += 1
                if l == "end":
                    break
                if not l or l.startswith("//"):
                    continue
                ch, rest = l.split("=", 1)
                ch = ch.strip()
                if len(ch) != 1:
                    die(f"{path}: palette key must be one character: {ch!r}")
                parts = rest.split()
                if kind == "palette":
                    table[ch] = parse_color(parts[0])
                else:
                    table[ch] = (int(parts[0]), parse_color(parts[1]))
            (palettes if kind == "palette" else ramps)[name] = table
        elif kind == "sprite":
            name, pal = words[1], words[2]
            rows = []
            while i < len(lines):
                l = lines[i].strip()
                i += 1
                if l == "end":
                    break
                if l:
                    rows.append(l)
            if not rows:
                die(f"{path}: sprite {name} is empty")
            w = max(len(r) for r in rows)
            rows = [r.ljust(w, ".") for r in rows]
            sprites.append((name, pal, rows, os.path.basename(path)))
        elif kind == "font":
            name = words[1]
            glyphs = {}
            while i < len(lines):
                l = lines[i].strip()
                i += 1
                if l == "end":
                    break
                if not l.startswith("glyph"):
                    continue
                key = l[len("glyph"):].strip()
                if key.startswith("\\x"):
                    code = int(key[2:], 16)
                elif key == "space":
                    code = 32
                elif len(key) == 1:
                    code = ord(key)
                else:
                    die(f"{path}: bad glyph key {key!r}")
                rows = []
                while len(rows) < 8 and i < len(lines):
                    rows.append(lines[i].strip())
                    i += 1
                glyphs[code] = rows
            fonts[name] = glyphs
        else:
            die(f"{path}: unknown block {kind!r} in line {line!r}")


for fn in sorted(os.listdir(ART)):
    if fn.endswith(".txt"):
        parse_file(os.path.join(ART, fn))

# Global palette: every fixed colour used by any sprite; index 0 means transparent.
gpal = [(0, 0, 0)]
gidx = {}


def global_index(c):
    if c not in gidx:
        gidx[c] = len(gpal)
        gpal.append(c)
    return gidx[c]


out_sprites = []
for name, pal, rows, src in sprites:
    h, w = len(rows), len(rows[0])
    data = []
    if pal in palettes:
        table, mode = palettes[pal], 0
        for r in rows:
            for ch in r:
                if ch == ".":
                    data.append(0)
                elif ch in table:
                    data.append(global_index(table[ch]))
                else:
                    die(f"{src}: sprite {name}: '{ch}' not in palette {pal}")
    elif pal in ramps:
        table, mode = ramps[pal], 1
        for r in rows:
            for ch in r:
                if ch == ".":
                    data.append(0)
                elif ch in table:
                    data.append(table[ch][0])
                else:
                    die(f"{src}: sprite {name}: '{ch}' not in ramp {pal}")
    else:
        die(f"{src}: sprite {name}: unknown palette {pal}")
    out_sprites.append((name, w, h, mode, pal, data, src))
if len(gpal) > 256:
    die(f"too many colours: {len(gpal)}")

os.makedirs(GEN, exist_ok=True)
with open(os.path.join(GEN, "art_data.h"), "w") as f:
    f.write("/* Generated by tools/artgen.py from the art folder - do not edit. */\n#pragma once\n#include <stdint.h>\n\n")
    f.write("typedef struct { uint16_t w, h; uint8_t ramp; const uint8_t *px; } Sprite;\n")
    f.write("typedef struct { uint8_t w; uint8_t rows[8]; } Glyph;\n\n")
    f.write("enum {\n")
    for n, *_ in out_sprites:
        f.write(f"    SPR_{n.upper()},\n")
    f.write("    SPR_COUNT\n};\n\n")
    f.write("extern const Sprite sprites[SPR_COUNT];\n")
    f.write(f"extern const uint16_t global_palette[{len(gpal)}];\n")
    for fname in fonts:
        f.write(f"extern const Glyph font_{fname}[128];\n")
    for rname in ramps:
        f.write(f"extern const uint16_t ramp_default_{rname}[16];\n")

with open(os.path.join(GEN, "art_data.c"), "w") as f:
    f.write("/* Generated by tools/artgen.py from the art folder - do not edit. */\n#include \"art_data.h\"\n\n")
    f.write(f"const uint16_t global_palette[{len(gpal)}] = {{\n")
    for k in range(0, len(gpal), 8):
        f.write("    " + ", ".join(f"0x{rgb565(c):04X}" for c in gpal[k:k + 8]) + ",\n")
    f.write("};\n\n")
    for rname, table in ramps.items():
        slots = [0] * 16
        for ch, (slot, col) in table.items():
            slots[slot] = rgb565(col)
        f.write(f"const uint16_t ramp_default_{rname}[16] = {{ " + ", ".join(f"0x{v:04X}" for v in slots) + " };\n")
    f.write("\n")
    for n, w, h, mode, pal, data, src in out_sprites:
        f.write(f"static const uint8_t px_{n}[{w * h}] = {{")
        for k in range(0, len(data), 32):
            f.write("\n    " + ",".join(str(v) for v in data[k:k + 32]) + ",")
        f.write("\n};\n")
    f.write("\nconst Sprite sprites[SPR_COUNT] = {\n")
    for n, w, h, mode, *_ in out_sprites:
        f.write(f"    {{ {w}, {h}, {mode}, px_{n} }},\n")
    f.write("};\n\n")
    for fname, glyphs in fonts.items():
        f.write(f"const Glyph font_{fname}[128] = {{\n")
        for code in range(128):
            rows = glyphs.get(code)
            if not rows:
                f.write("    { 0, {0} },\n")
                continue
            w = 3 if code == 32 else max((len(r.rstrip(".")) for r in rows), default=0)
            bits = []
            for r in rows:
                v = 0
                for x, ch in enumerate(r):
                    if ch == "#":
                        v |= 1 << x
                bits.append(v)
            f.write(f"    {{ {w}, {{ {', '.join(str(b) for b in bits)} }} }}, /* {code} */\n")
        f.write("};\n")


def write_png(path, w, h, pixels):
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            raw.extend(pixels[y * w + x])

    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
    open(path, "wb").write(png)


# Previews: one sheet per source file, sprites laid out in rows at 4x on a checkerboard.
os.makedirs(PREV, exist_ok=True)
SCALE, PAD = 4, 6
by_file = {}
for s in out_sprites:
    by_file.setdefault(s[6], []).append(s)
for src, items in by_file.items():
    rows_out, cur, cur_w = [], [], PAD
    for it in items:
        iw = it[1] * SCALE + PAD
        if cur and cur_w + iw > 1400:
            rows_out.append(cur)
            cur, cur_w = [], PAD
        cur.append(it)
        cur_w += iw
    rows_out.append(cur)
    W = max(sum(it[1] * SCALE + PAD for it in r) + PAD for r in rows_out)
    heights = [max(it[2] for it in r) * SCALE + PAD for r in rows_out]
    H = sum(heights) + PAD
    px = [((60, 56, 70) if ((x // 8 + y // 8) % 2) else (78, 72, 90)) for y in range(H) for x in range(W)]
    oy = PAD
    for r, rh in zip(rows_out, heights):
        ox = PAD
        for n, w, h, mode, pal, data, _ in r:
            slot_col = {slot: col for ch, (slot, col) in ramps[pal].items()} if mode == 1 else {}
            for y in range(h):
                for x in range(w):
                    v = data[y * w + x]
                    if not v:
                        continue
                    col = gpal[v] if mode == 0 else slot_col.get(v, (255, 0, 255))
                    for dy in range(SCALE):
                        base = (oy + y * SCALE + dy) * W + ox + x * SCALE
                        for dx in range(SCALE):
                            px[base + dx] = col
            ox += w * SCALE + PAD
        oy += rh
    write_png(os.path.join(PREV, src.replace(".txt", ".png")), W, H, px)

print(f"artgen: {len(out_sprites)} sprites, {len(gpal) - 1} colours, fonts {list(fonts)}, ramps {list(ramps)}")
