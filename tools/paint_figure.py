"""Full-body figures for Thin Walls' title and pick screens (build 06). Called by paint_bust.py, which owns the
Canvas class and the portrait drawings; returns sprite blocks for art/bust.txt.

A figure is 56x128. Its head is the portrait's own layers at half size (32x32, drawn at x 12, y 0): face, hair,
ears in every pose and the accessory, reduced so the figure is the same character, plus small eyes and mouths
painted at this size. The body layers are the whole figure from the neck down, skin and clothes together, one per
outfit, in the stance her card gives her: Nia with her sleeves pulled over her hands, Mako with a hand on her hip,
Shio holding her own wrist. About 5.5 heads tall: petite adults, never chibi."""
import math

FW, FH = 56, 128
HX = 12                                   # the head canvas sits at (HX, 0) in the figure


def halve(Canvas, src, priority="ajhwpHsdoOqie"):
    """Half-size copy of a portrait layer: the outline becomes its neighbour's colour, each 2x2 block takes its
    majority colour (ties by priority), then the silhouette is outlined again at the new size."""
    fill = [[src.get(x, y) for x in range(src.w)] for y in range(src.h)]
    for y in range(src.h):
        for x in range(src.w):
            if fill[y][x] == "k":
                nb = [src.get(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))]
                nb = [c for c in nb if c not in ".k"]
                fill[y][x] = max(sorted(set(nb)), key=nb.count) if nb else "."
    out = Canvas(src.w // 2, src.h // 2)
    for y in range(out.h):
        for x in range(out.w):
            cells = [fill[2 * y + j][2 * x + i] for j in (0, 1) for i in (0, 1)]
            solid = [c for c in cells if c != "."]
            if len(solid) < 2:
                continue
            rank = {c: (solid.count(c), -priority.find(c) if c in priority else -99) for c in set(solid)}
            out.set(x, y, max(rank, key=rank.get))
    out.outline("k")
    return out


def points_half(Canvas, src):
    """Half-size copy of a thin line drawing (the headband, the hair clip): every pixel lands at half its place."""
    out = Canvas(src.w // 2, src.h // 2)
    for y in range(src.h):
        for x in range(src.w):
            c = src.get(x, y)
            if c != "." and out.get(x // 2, y // 2) in ".k":
                out.set(x // 2, y // 2, c)
    return out


def line(c, x0, y0, x1, y1, ch, only_over=None):
    n = int(max(abs(x1 - x0), abs(y1 - y0))) + 1
    for k in range(n + 1):
        t = k / n
        x, y = int(round(x0 + (x1 - x0) * t)), int(round(y0 + (y1 - y0) * t))
        if only_over is None or c.get(x, y) in only_over:
            c.set(x, y, ch)


def stroke(c, pts, ch, only_over=None, closed=False):
    for i in range(len(pts) - (0 if closed else 1)):
        (x0, y0), (x1, y1) = pts[i], pts[(i + 1) % len(pts)]
        line(c, x0, y0, x1, y1, ch, only_over)


def mirror_pts(pts):
    return [(FW - x, y) for x, y in pts]


# ---------------------------------------------------------------- small eyes and mouths, in head coordinates
# The left eye's pixels (outer corner on the left) as rows from (8, 16); the right eye is its mirror image.
EYES = {
    "neutral": [".kkkkk", "kk...k", ".wiiie", ".iiiie", "..iii."],
    "wide":    [".kkkkk", "k.w..k", ".wiiw.", ".wiiw.", "..ww.."],
    "sparkle": [".kkkkk", "kk...k", ".wiwie", ".iwwwe", "..iwi."],
    "half":    ["......", "......", ".kkkkk", "kkeeek", "..iii."],
    "happy":   ["......", "..kk..", ".k..k.", "k....k", "......"],
    "closed":  ["......", "......", "......", ".kkkkk", "k....."],
    "aside":   [".kkkkk", "kk...k", ".iiww.", ".iiiw.", "..ii.."],
}


def eyes_small(Canvas, kind):
    """Both eyes: the right one is the left one mirrored, except that a sideways glance keeps its direction."""
    c = Canvas(32, 32)
    for y, r in enumerate(EYES[kind]):
        right = r if kind == "aside" and y >= 2 else r[::-1]
        for x in range(6):
            for ch, cx in ((r[x], 8 + x), (right[x], 18 + x)):
                if ch == ".":
                    continue
                if ch == "i" and y <= 2 and kind != "half":
                    ch = "e"                      # the top of the iris is dark, as in the portraits
                c.set(cx, 16 + y, ch)
    return c


MOUTHS = {                                        # rows from (14, 23)
    "cat":   ["k...k", ".k.k.", "..k.."][:2],
    "smile": ["k...k", ".kkk."],
    "flat":  [".....", ".kkk."],
    "pout":  ["..k..", ".k.k."],
    "open":  [".kkk.", ".kmk.", "..k.."],
    "smirk": [".....", "..kkk", ".k..."],
    "fang":  ["k...k", ".kwk."],
}


def mouth_small(Canvas, kind):
    c = Canvas(32, 32)
    for y, r in enumerate(MOUTHS[kind]):
        for x, ch in enumerate(r):
            if ch != ".":
                c.set(14 + x, 23 + y, ch)
    return c


def blush_small(Canvas):
    c = Canvas(32, 32)
    for x0 in (8, 20):
        for (dx, dy) in ((0, 22), (1, 21), (2, 22), (3, 21)):
            c.set(x0 + dx, dy, "p")
    return c


def glasses_small(Canvas):
    c = Canvas(32, 32)
    for cx in (10.5, 21.5):
        for a in range(0, 360, 10):
            x = int(round(cx + 3.4 * math.cos(math.radians(a))))
            y = int(round(19 + 2.9 * math.sin(math.radians(a))))
            c.set(x, y, "k")
    for x in (15, 16):
        c.set(x, 18, "k")
    c.set(8, 17, "w")
    c.set(23, 20, "p")
    return c


# ---------------------------------------------------------------- bodies
# Figure coordinates: centre x 28, chin at y 26, shoulders y 30, waist y 48, hips y 58, knees y 90, soles y 122.
TORSO = [(20.5, 29), (35.5, 29), (38.5, 32), (38, 42), (35, 48), (34.5, 51), (38.5, 57), (39, 62), (17, 62),
         (17.5, 57), (21.5, 51), (21, 48), (18, 42), (17.5, 32)]
# a leg with a shape: full thigh, the knee narrowing, a calf, a slim ankle
LEG_L = [(17.4, 60), (28, 60), (27.9, 68), (27.3, 80), (26.7, 88), (27, 95), (26.4, 104), (25.5, 112), (25.3, 117),
         (21.4, 117), (21, 112), (19.8, 104), (19.3, 95), (19.8, 88), (19, 80), (18.1, 68)]
LEG_R = mirror_pts(LEG_L)
SHADE = {"O": "q", "o": "O", "s": "d"}
LIGHT = {"O": "o"}


def arms_for(who):
    """(upper arm, forearm, hand) polygons for each arm, in the stance from her card."""
    if who == "nia":                              # shy: arms in, hands together in front, sleeves over them
        left = ([(17.5, 30), (22.5, 32), (21.5, 45), (16.5, 45)], [(16.5, 44.5), (21.5, 45), (28.5, 54.5), (25, 58.5)],
                [(24.5, 55.5), (28.5, 55), (29, 59), (25, 59.5)])
        right = tuple(mirror_pts(p) for p in left)
        return left, right
    if who == "mako":                             # confident: her left hand on her hip, elbow out
        right = ([(17.5, 30), (22, 32.5), (21, 46), (16, 46)], [(16, 45.5), (21, 46), (20.5, 57.5), (15.8, 57.5)],
                 [(15.8, 57), (20.2, 57), (20, 61.5), (16.5, 62)])
        left = ([(34.5, 30), (39, 31), (46, 42), (42.5, 45.5)], [(42.5, 44.5), (46, 42), (40.5, 53.5), (37, 53)],
                [(36, 51), (40, 52), (39.5, 55.5), (35.5, 55)])
        return right, left
    left = ([(17.5, 30), (22, 32.5), (22, 45), (17, 45)], [(17, 44.5), (22, 45), (28, 50.5), (25.8, 54.5)],
            [(24.5, 50.5), (28.5, 50), (29, 54), (25, 55)])     # Shio: hands at the waist, one around the other wrist
    right = ([(38.5, 30), (34, 32.5), (34, 45), (39, 45)], [(39, 44.5), (34, 45), (29, 50.5), (31.7, 54.5)],
             [(27.5, 50), (31.5, 50.5), (31, 54.5), (27, 54)])
    return left, right


def fill_arm(c, arm, sleeve, cuff=None, bare_hand=True):
    upper, fore, hand = arm
    c.poly(upper, sleeve)
    c.poly(fore, sleeve)
    if bare_hand:
        c.poly(hand, "s")
    if cuff:
        a, b = fore[2], fore[3]
        line(c, a[0], a[1], b[0], b[1], cuff)
    edge = "k" if sleeve == "s" else "q"
    stroke(c, upper + [upper[0]], edge, only_over=".sOoqwaej" + sleeve)
    stroke(c, [fore[0], fore[3]], edge, only_over="sOoqwaej")
    stroke(c, [fore[1], fore[2]], edge, only_over="sOoqwaej")


def legs(c, skin_to, sock=None, sock_from=None):
    """Bare legs from skin_to down; socks or tights from sock_from."""
    for pts in (LEG_L, LEG_R):
        c.poly(pts, "s")
    if sock:
        for y in range(sock_from, 118):
            for x in range(FW):
                if c.get(x, y) == "s":
                    c.set(x, y, sock)
    line(c, 28, 62, 28, 116, ".")                 # the gap between the legs


def shoes(c, ch, sole="q"):
    for sgn in (1, -1):
        pts = [(20.5, 115.5), (26, 115.5), (26.5, 121.5), (18.5, 122.5), (18, 120)]
        if sgn < 0:
            pts = mirror_pts(pts)
        c.poly(pts, ch)
        y = 122
        for x in range(17, 40):
            if c.get(x, y - 1) == ch:
                c.set(x, y, sole)


def shade_pass(c, top=26):
    """Light from the viewer's left: one lighter column on the left edge of each garment, a darker one on the right."""
    marks = []
    for y in range(top, FH):
        for x in range(FW):
            ch = c.get(x, y)
            if ch in SHADE and c.get(x + 1, y) in ".k" and c.get(x - 1, y) == ch:
                marks.append((x, y, SHADE[ch]))
            elif ch in LIGHT and c.get(x - 1, y) in ".k" and c.get(x + 1, y) == ch:
                marks.append((x, y, LIGHT[ch]))
    for x, y, ch in marks:
        c.set(x, y, ch)


def neck(c):
    c.poly([(25.5, 24), (30.5, 24), (30.5, 31), (25.5, 31)], "s")
    for x in range(26, 31):
        c.set(x, 29, "d")


def body(Canvas, who, outfit):
    c = Canvas(FW, FH)
    L, R = arms_for(who)
    neck(c)
    if who == "nia":
        if outfit == "hoodie":                    # oversized hoodie to mid-thigh, shorts under it, knee socks
            legs(c, 74, "j", 94)
            c.poly([(18, 72), (38, 72), (38, 76), (18, 76)], "e")
            c.poly([(19, 28), (37, 28), (40, 33), (40.5, 60), (41.5, 74), (14.5, 74), (15.5, 60), (16, 33)], "O")
            c.ellipse(28, 30, 8.5, 3.2, "o")
            c.ellipse(28, 30.6, 4, 1.6, "q")
            for y in range(32, 38):
                c.set(26, y, "w"); c.set(30, y, "w")
            stroke(c, [(21, 64), (35, 64)], "q")
            stroke(c, [(21, 64), (22, 57), (34, 57), (35, 64)], "q")
            stroke(c, [(15, 71), (41, 71)], "q")
            for arm in (L, R):
                fill_arm(c, arm, "O", "o", bare_hand=False)
            for (x, y) in ((26, 59), (27, 60), (29, 60), (30, 59)):
                c.set(x, y, "s")
            shoes(c, "w")
        elif outfit == "sweater":                 # slouchy knit off one shoulder, pleated skirt, dark socks
            legs(c, 72, "j", 96)
            c.poly([(17.5, 56), (38.5, 56), (41, 73), (15, 73)], "e")
            for x in (21, 25, 29, 33, 37):
                line(c, x, 58, x + (x - 28) * 0.15, 72, "k", only_over="e")
            c.poly(TORSO, "s")
            c.poly([(16, 33), (24, 29.5), (37, 29), (40, 33), (40.5, 58), (15.5, 58)], "o")
            stroke(c, [(16, 33), (24, 29.5), (33, 30)], "O")
            for x in range(18, 39, 3):
                for y in range(52, 58):
                    if c.get(x, y) == "o":
                        c.set(x, y, "O")
            for arm in (L, R):
                fill_arm(c, arm, "o", "O", bare_hand=False)
            for (x, y) in ((26, 59), (27, 60), (29, 60), (30, 59)):
                c.set(x, y, "s")
            shoes(c, "e")
        else:                                     # "tank": hoodie open over a white tank top, shorts
            legs(c, 70, None, None)
            c.poly([(18, 58), (38, 58), (38.5, 70), (17.5, 70)], "q")
            c.poly(TORSO, "o")
            c.poly([(19, 28), (25, 29), (25.5, 62), (16, 62), (16, 33)], "O")
            c.poly(mirror_pts([(19, 28), (25, 29), (25.5, 62), (16, 62), (16, 33)]), "O")
            c.ellipse(28, 30, 8.5, 3, "O")
            c.poly([(24.5, 30), (31.5, 30), (31, 34), (25, 34)], "s")
            stroke(c, [(24.5, 34), (31.5, 34)], "d")
            for arm in (L, R):
                fill_arm(c, arm, "O", "q", bare_hand=False)
            for (x, y) in ((26, 59), (27, 60), (29, 60), (30, 59)):
                c.set(x, y, "s")
            shoes(c, "o")
    elif who == "mako":
        if outfit == "jacket":                    # cropped jacket over a light top, pleated skirt, knee socks
            legs(c, 72, "O", 98)
            c.poly([(17.5, 55), (38.5, 55), (41.5, 74), (14.5, 74)], "q")
            for x in (20, 24, 28, 32, 36):
                line(c, x, 57, x + (x - 28) * 0.2, 73, "O", only_over="q")
            c.poly(TORSO, "o")
            c.poly([(18, 30), (24.5, 29.5), (26, 45), (20.5, 46), (18.5, 43)], "O")
            c.poly(mirror_pts([(18, 30), (24.5, 29.5), (26, 45), (20.5, 46), (18.5, 43)]), "O")
            for x in range(25, 32):
                c.set(x, 32 + (1 if 26 <= x <= 30 else 0), "a")
            for arm in (L, R):
                fill_arm(c, arm, "O", "o")
            shoes(c, "O", "o")
        elif outfit == "sport":                   # sports crop top, bare midriff, track shorts with a stripe
            legs(c, 70, "O", 106)
            c.poly([(18, 51), (38, 51), (39.5, 69), (16.5, 69)], "O")
            line(c, 17, 53, 16.8, 68, "o")
            line(c, 39, 53, 39.3, 68, "o")
            c.poly(TORSO, "s")
            c.poly([(19, 32), (37, 32), (37.5, 42), (18.5, 42)], "o")
            stroke(c, [(18.5, 42), (37.5, 42)], "O")
            stroke(c, [(22, 29), (20, 32)], "O")
            stroke(c, [(34, 29), (36, 32)], "O")
            for y in (46, 47):
                c.set(28, y, "d")
            for x in range(25, 32):
                c.set(x, 31 + (1 if 26 <= x <= 30 else 0), "a")
            for arm in (L, R):
                fill_arm(c, arm, "s")
            shoes(c, "o", "O")
        else:                                     # "dress": sleeveless going-out dress, heeled boots
            legs(c, 76, "q", 102)
            c.poly(TORSO, "s")
            c.poly([(19.5, 34), (36.5, 34), (37.5, 43), (34.5, 48), (38.5, 58), (42, 77), (14, 77), (17.5, 58),
                    (21.5, 48), (18.5, 43)], "O")
            for t in range(6):
                c.set(21 + t // 3, 29 + t, "q"); c.set(35 - t // 3, 29 + t, "q")
            stroke(c, [(21.5, 48), (34.5, 48)], "q")
            for x in (22, 28, 34):
                line(c, x, 60, x + (x - 28) * 0.3, 76, "q", only_over="O")
            for x in range(25, 32):
                c.set(x, 31 + (1 if 26 <= x <= 30 else 0), "a")
            for arm in (L, R):
                fill_arm(c, arm, "s")
            shoes(c, "q", "e")
    else:
        if outfit == "cardigan":                  # long oatmeal cardigan over a collared shirt, long skirt, flats
            legs(c, 104, None, None)
            c.poly([(18.5, 56), (37.5, 56), (40, 104), (16, 104)], "q")
            c.poly(TORSO, "w")
            c.poly([(17, 30), (25, 29.5), (26.5, 70), (15.5, 70), (16, 33)], "o")
            c.poly(mirror_pts([(17, 30), (25, 29.5), (26.5, 70), (15.5, 70), (16, 33)]), "o")
            c.poly([(25, 29), (28, 33), (24, 33)], "w")
            c.poly([(31, 29), (28, 33), (32, 33)], "w")
            for y in (38, 44, 50):
                c.set(28, y, "q")
            for arm in (L, R):
                fill_arm(c, arm, "o", "O")
            shoes(c, "q", "e")
        elif outfit == "blouse":                  # pressed blouse tucked into a pencil skirt
            legs(c, 88, None, None)
            c.poly([(20, 50), (36, 50), (37.5, 58), (36.5, 88), (19.5, 88), (18.5, 58)], "e")
            c.poly([(18, 30), (38, 30), (38, 43), (35, 50), (21, 50), (18, 43)], "o")
            c.poly([(26, 29.5), (30, 29.5), (28, 34)], "s")
            for y in (37, 42, 47):
                c.set(28, y, "O")
            stroke(c, [(28, 34), (28, 50)], "O")
            for arm in (L, R):
                fill_arm(c, arm, "o", "O")
            shoes(c, "e", "q")
        else:                                     # "sundress": straps, fitted bodice, flared skirt, sandals
            legs(c, 90, None, None)
            c.poly(TORSO, "s")
            c.poly([(19.5, 35), (36.5, 35), (37.5, 43), (34.5, 49), (38, 58), (42, 90), (14, 90), (18, 58),
                    (21.5, 49), (18.5, 43)], "o")
            for x in range(15, 42):
                if c.get(x, 89) == "o":
                    c.set(x, 89 - (x % 3 == 0), "w")
            for t in range(6):
                c.set(22, 29 + t, "O"); c.set(34, 29 + t, "O")
            stroke(c, [(21.5, 49), (34.5, 49)], "O")
            for x in (21, 28, 35):
                line(c, x, 62, x + (x - 28) * 0.35, 88, "O", only_over="o")
            for arm in (L, R):
                fill_arm(c, arm, "s")
            shoes(c, "O", "q")
    shade_pass(c)
    c.outline("k")
    return c


def blocks(Canvas, sprite, face, hair_fns, one_ear, ear_poses, band, clip):
    out = [sprite("fig_face", halve(Canvas, face))]
    for kind in ("neutral", "happy", "half", "wide", "closed", "sparkle", "aside"):
        out.append(sprite(f"fig_eyes_{kind}", eyes_small(Canvas, kind)))
    for kind in ("cat", "smile", "flat", "pout", "open", "smirk", "fang"):
        out.append(sprite(f"fig_mouth_{kind}", mouth_small(Canvas, kind)))
    out.append(sprite("fig_blush", blush_small(Canvas)))
    for name in ("nia", "mako", "shio"):
        out.append(sprite(f"fig_hair_{name}", halve(Canvas, hair_fns[name]())))
    for pose in ear_poses:
        out.append(sprite(f"fig_ear_{pose}", halve(Canvas, one_ear(pose))))
    out.append(sprite("fig_acc_glasses", glasses_small(Canvas)))
    out.append(sprite("fig_acc_headband", points_half(Canvas, band)))
    out.append(sprite("fig_acc_clip", points_half(Canvas, clip)))
    for who, outfits in (("nia", ("hoodie", "sweater", "tank")), ("mako", ("jacket", "sport", "dress")),
                         ("shio", ("cardigan", "blouse", "sundress"))):
        for o in outfits:
            out.append(sprite(f"fig_body_{who}_{o}", body(Canvas, who, o)))
    return out
