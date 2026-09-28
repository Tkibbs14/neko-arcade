#!/usr/bin/env python3
"""Pixel icon for the TreeFrogUI system list (Pixel_by_Jeltron pack style: a small transparent device):
a cat-eared arcade cabinet. Writes build/stick/nekoarcade_icon.png (48x64 RGBA)."""
import os, struct, zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
W, H = 48, 64
INK, PINK, ROSE, EAR = (42, 31, 51, 255), (255, 122, 168, 255), (217, 70, 126, 255), (255, 190, 210, 255)
NAVY, MINT, GOLD, SKY, RED = (26, 22, 48, 255), (122, 224, 192, 255), (255, 208, 96, 255), (106, 168, 255, 255), (255, 90, 110, 255)
img = [[(0, 0, 0, 0)] * W for _ in range(H)]


def px(x, y, c):
    if 0 <= x < W and 0 <= y < H:
        img[y][x] = c


def rect(x, y, w, h, c):
    for j in range(y, y + h):
        for i in range(x, x + w):
            px(i, j, c)


def box(x, y, w, h, fill):           # filled rectangle with a 1px ink outline
    rect(x - 1, y - 1, w + 2, h + 2, INK)
    rect(x, y, w, h, fill)


def ear(x0, x1, tip_x, top, base, fill, inner):
    for y in range(top, base + 1):
        t = (y - top) / max(1, base - top)
        lo, hi = round(tip_x + (x0 - tip_x) * t), round(tip_x + (x1 - tip_x) * t)
        for x in range(lo - 1, hi + 2):
            px(x, y, INK)
    for y in range(top + 2, base + 1):
        t = (y - top) / max(1, base - top)
        lo, hi = round(tip_x + (x0 - tip_x) * t), round(tip_x + (x1 - tip_x) * t)
        for x in range(lo, hi + 1):
            px(x, y, fill)
        if y > top + 4:
            for x in range(lo + 2, hi - 1):
                px(x, y, inner)


ear(7, 19, 9, 2, 14, PINK, EAR)
ear(28, 40, 38, 2, 14, PINK, EAR)
box(6, 12, 36, 10, PINK)             # marquee
rect(9, 14, 30, 6, GOLD)
for i in range(4):
    px(13 + i * 7, 16, ROSE)
    px(14 + i * 7, 17, ROSE)
box(6, 23, 36, 19, ROSE)             # screen bezel
rect(9, 25, 30, 15, NAVY)
for (x, y) in [(15, 29), (16, 28), (17, 29), (30, 29), (31, 28), (32, 29)]:   # happy cat eyes on the screen
    px(x, y, MINT)
for (x, y) in [(21, 33), (22, 34), (23, 33), (24, 34), (25, 33)]:            # :3 mouth
    px(x, y, PINK)
for x in (12, 35):
    px(x, 32, EAR)
box(4, 43, 40, 7, PINK)              # control panel
rect(12, 40, 2, 5, INK)              # joystick
rect(11, 39, 4, 2, RED)
px(12, 38, RED); px(13, 38, RED)
box(26, 45, 3, 2, GOLD)
box(32, 45, 3, 2, SKY)
box(8, 51, 32, 11, PINK)             # base with the coin slot
rect(21, 54, 6, 5, ROSE)
rect(23, 55, 2, 3, INK)
rect(9, 60, 30, 1, ROSE)

raw = b"".join(b"\x00" + bytes(v for p in row for v in p) for row in img)
png = b"\x89PNG\r\n\x1a\n"
for tag, data in ((b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 6, 0, 0, 0)), (b"IDAT", zlib.compress(raw, 9)), (b"IEND", b"")):
    png += struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
out = os.path.join(ROOT, "build", "stick", "nekoarcade_icon.png")
open(out, "wb").write(png)
print(f"mkicon: {out} ({len(png)} bytes)")
