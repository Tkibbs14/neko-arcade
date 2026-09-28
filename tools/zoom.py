#!/usr/bin/env python3
"""Crop and upscale one of our own PNGs (8-bit RGB, filter 0) for close inspection.
Usage: zoom.py IN.png OUT.png SCALE [x y w h]"""
import struct, sys, zlib


def read_png(path):
    data = open(path, "rb").read()
    pos, w, h, idat = 8, 0, 0, b""
    while pos < len(data):
        n, t = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        if t == b"IHDR":
            w, h = struct.unpack(">II", body[:8])
        elif t == b"IDAT":
            idat += body
        pos += 12 + n
    raw = zlib.decompress(idat)
    rows = []
    stride = 3 * w
    for y in range(h):
        line = raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)]
        rows.append([tuple(line[3 * x:3 * x + 3]) for x in range(w)])
    return w, h, rows


def write_png(path, w, h, rows):
    raw = b"".join(b"\0" + bytes(c for px in r for c in px) for r in rows)
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                           + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


src, dst, s = sys.argv[1], sys.argv[2], int(sys.argv[3])
w, h, rows = read_png(src)
x, y, cw, ch = (int(v) for v in sys.argv[4:8]) if len(sys.argv) >= 8 else (0, 0, w, h)
out = []
for yy in range(y, y + ch):
    line = [px for px in rows[yy][x:x + cw] for _ in range(s)]
    out.extend([line] * s)
write_png(dst, cw * s, ch * s, out)
