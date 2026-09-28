#include "chibi.h"

#define SKIN RGB(255, 230, 212)
#define LINE_C RGB(58, 40, 62)
#define BLUSH RGB(255, 150, 172)
#define MOUTH RGB(160, 58, 82)
#define WHITE RGB(255, 252, 248)

/* hair, hair dark, ear, inner ear, eye, outfit, outfit dark, accent, ears, hair style,
 * whiskers, glasses, tanuki mask, apron (in the accent colour) */
const ChibiLook chibi_looks[LK_COUNT] = {
    [LK_HIRE] = { RGB(66, 56, 82), RGB(42, 34, 56), RGB(66, 56, 82), RGB(255, 172, 192), RGB(64, 176, 124),
                  RGB(246, 242, 236), RGB(96, 88, 110), RGB(92, 154, 112), EARS_CAT, HAIR_BOB, 0, 0, 0, 1 },
    [LK_HIRE2] = { RGB(244, 240, 246), RGB(196, 186, 212), RGB(244, 240, 246), RGB(255, 172, 192), RGB(214, 110, 170),
                   RGB(246, 242, 236), RGB(96, 88, 110), RGB(232, 120, 164), EARS_CAT, HAIR_TWIN, 0, 0, 0, 1 },
    [LK_MOCHA] = { RGB(200, 142, 92), RGB(150, 100, 64), RGB(200, 142, 92), RGB(255, 190, 170), RGB(220, 150, 60),
                   RGB(250, 244, 232), RGB(120, 96, 80), RGB(150, 182, 140), EARS_CAT, HAIR_PONY, 0, 0, 0, 1 },
    [LK_NIA] = { RGB(44, 40, 60), RGB(84, 120, 222), RGB(44, 40, 60), RGB(255, 172, 192), RGB(156, 112, 226),
                 RGB(150, 150, 164), RGB(96, 96, 110), RGB(84, 120, 222), EARS_CAT, HAIR_LONG, 0, 0, 0, 0 },
    [LK_MAKO] = { RGB(224, 74, 162), RGB(160, 40, 112), RGB(224, 74, 162), RGB(255, 196, 220), RGB(250, 196, 84),
                  RGB(112, 62, 112), RGB(76, 40, 80), RGB(84, 220, 230), EARS_CAT, HAIR_TWIN, 0, 0, 0, 0 },
    [LK_SHIO] = { RGB(146, 222, 200), RGB(92, 170, 150), RGB(250, 250, 250), RGB(146, 222, 200), RGB(112, 140, 172),
                  RGB(224, 204, 172), RGB(170, 150, 120), RGB(146, 222, 200), EARS_CAT, HAIR_BOB, 0, 1, 0, 0 },
    [LK_KURO] = { RGB(52, 50, 62), RGB(150, 150, 160), RGB(52, 50, 62), RGB(200, 150, 160), RGB(232, 200, 84),
                  RGB(92, 112, 142), RGB(60, 74, 100), RGB(232, 200, 120), EARS_CAT, HAIR_SHORT, 1, 0, 0, 0 },
    [LK_SUZU] = { RGB(252, 244, 234), RGB(232, 142, 62), RGB(232, 142, 62), RGB(255, 190, 200), RGB(122, 184, 92),
                  RGB(244, 184, 204), RGB(200, 130, 160), RGB(255, 226, 90), EARS_CAT, HAIR_BOB, 0, 0, 0, 1 },
    [LK_TSUKI] = { RGB(92, 92, 124), RGB(62, 62, 92), RGB(92, 92, 124), RGB(230, 170, 200), RGB(150, 150, 206),
                   RGB(72, 82, 112), RGB(50, 56, 80), RGB(236, 236, 244), EARS_CAT, HAIR_LONG, 0, 0, 0, 0 },
    [LK_PIPI] = { RGB(252, 234, 204), RGB(226, 196, 160), RGB(252, 244, 236), RGB(255, 180, 196), RGB(232, 100, 132),
                  RGB(252, 172, 204), RGB(214, 120, 160), RGB(255, 255, 255), EARS_BUNNY, HAIR_TWIN, 0, 0, 0, 0 },
    [LK_WHISKERS] = { RGB(176, 176, 182), RGB(122, 122, 132), RGB(150, 150, 158), RGB(220, 170, 176), RGB(204, 180, 92),
                      RGB(134, 104, 82), RGB(96, 74, 58), RGB(184, 42, 62), EARS_CAT, HAIR_SHORT, 1, 0, 0, 0 },
    [LK_KITSU] = { RGB(242, 142, 62), RGB(192, 92, 40), RGB(242, 142, 62), RGB(255, 236, 220), RGB(204, 140, 60),
                   RGB(252, 244, 234), RGB(210, 196, 184), RGB(192, 92, 40), EARS_FOX, HAIR_LONG, 0, 0, 0, 0 },
    [LK_HANA] = { RGB(204, 152, 102), RGB(150, 102, 62), RGB(150, 102, 62), RGB(150, 102, 62), RGB(112, 82, 62),
                  RGB(112, 172, 232), RGB(70, 120, 180), RGB(255, 255, 255), EARS_DOG, HAIR_PONY, 0, 0, 0, 0 },
    [LK_PONTA] = { RGB(152, 122, 92), RGB(84, 66, 56), RGB(152, 122, 92), RGB(84, 66, 56), RGB(40, 30, 30),
                   RGB(172, 152, 112), RGB(130, 112, 80), RGB(96, 176, 96), EARS_TANUKI, HAIR_SHORT, 0, 0, 1, 0 },
};

static void ears(const ChibiLook *k, int cx, int cy)
{
    for (int s = -1; s <= 1; s += 2) {
        switch (k->ears) {
        case EARS_CAT:
            tri_fill(cx + s * 9, cy - 2, cx + s * 7, cy - 15, cx + s * 1, cy - 8, LINE_C);
            tri_fill(cx + s * 8, cy - 3, cx + s * 7, cy - 13, cx + s * 2, cy - 8, k->ear);
            tri_fill(cx + s * 7, cy - 5, cx + s * 7, cy - 11, cx + s * 4, cy - 8, k->ear_in);
            break;
        case EARS_FOX:
            tri_fill(cx + s * 10, cy - 2, cx + s * 8, cy - 18, cx + s * 1, cy - 8, LINE_C);
            tri_fill(cx + s * 9, cy - 3, cx + s * 8, cy - 16, cx + s * 2, cy - 8, k->ear);
            tri_fill(cx + s * 8, cy - 6, cx + s * 8, cy - 12, cx + s * 4, cy - 8, k->ear_in);
            tri_fill(cx + s * 9, cy - 13, cx + s * 8, cy - 16, cx + s * 6, cy - 13, k->hair_dark);
            break;
        case EARS_BUNNY:
            ellipse_fill(cx + s * 4, cy - 15, 3, 9, LINE_C);
            ellipse_fill(cx + s * 4, cy - 15, 2, 8, k->ear);
            ellipse_fill(cx + s * 4, cy - 14, 1, 5, k->ear_in);
            break;
        case EARS_DOG:                                 /* floppy ears hang over the sides of the head */
            ellipse_fill(cx + s * 8, cy + 1, 4, 7, LINE_C);
            ellipse_fill(cx + s * 8, cy + 1, 3, 6, k->ear);
            break;
        case EARS_TANUKI:
            circle_fill(cx + s * 6, cy - 8, 4, LINE_C);
            circle_fill(cx + s * 6, cy - 8, 3, k->ear);
            circle_fill(cx + s * 6, cy - 8, 1, k->ear_in);
            break;
        }
    }
}

static void eyes(const ChibiLook *k, int cx, int ey, int expr)
{
    for (int s = -1; s <= 1; s += 2) {
        int ex = cx + s * 3;
        switch (expr) {
        case CE_HAPPY:
            pset(ex - 1, ey, LINE_C); pset(ex, ey - 1, LINE_C); pset(ex + 1, ey, LINE_C);
            break;
        case CE_SLEEPY:
            hline(ex - 1, ey, 3, LINE_C);
            break;
        case CE_SURPRISED:
            rect(ex - 1, ey - 1, 3, 3, WHITE);
            rect_line(ex - 1, ey - 1, 3, 3, LINE_C);
            pset(ex, ey, k->eye);
            break;
        default:
            hline(ex - 1, ey - 2, 3, LINE_C);
            rect(ex - 1, ey - 1, 2, 3, k->eye);
            pset(ex - 1, ey - 1, WHITE);
            pset(ex, ey + 1, blend(k->eye, LINE_C, 120));
            if (expr == CE_ANGRY)
                line(ex - s * 2, ey - 4, ex + s * 1, ey - 3, LINE_C);
            if (expr == CE_SAD)
                pset(ex + s, ey + 2, RGB(120, 190, 255));
            break;
        }
    }
}

static void mouth(int cx, int my, int expr)
{
    switch (expr) {
    case CE_HAPPY:
        rect(cx - 1, my, 3, 2, MOUTH);
        pset(cx, my + 1, BLUSH);
        break;
    case CE_ANGRY:
        hline(cx - 1, my + 1, 3, MOUTH);
        break;
    case CE_SLEEPY:
        pset(cx, my + 1, MOUTH);
        break;
    case CE_SURPRISED:
        rect(cx - 1, my, 2, 2, MOUTH);
        break;
    case CE_SAD:
        pset(cx - 1, my + 1, MOUTH); pset(cx, my, MOUTH); pset(cx + 1, my + 1, MOUTH);
        break;
    default:                                           /* the :3 mouth */
        pset(cx - 2, my, MOUTH); pset(cx - 1, my + 1, MOUTH); pset(cx, my, MOUTH);
        pset(cx + 1, my + 1, MOUTH); pset(cx + 2, my, MOUTH);
        break;
    }
}

void chibi_head(int look, int cx, int cy, int expr)
{
    const ChibiLook *k = &chibi_looks[look];
    if (k->style == HAIR_LONG)
        round_box(cx - 10, cy - 2, 20, 16, LINE_C, k->hair_dark == k->hair ? k->hair : blend(k->hair, LINE_C, 40));
    if (k->style == HAIR_TWIN)
        for (int s = -1; s <= 1; s += 2) {
            ellipse_fill(cx + s * 11, cy + 4, 3, 8, LINE_C);
            ellipse_fill(cx + s * 11, cy + 4, 2, 7, k->hair);
        }
    if (k->style == HAIR_PONY) {
        ellipse_fill(cx + 10, cy + 3, 4, 7, LINE_C);
        ellipse_fill(cx + 10, cy + 3, 3, 6, k->hair);
    }
    if (k->ears != EARS_DOG)
        ears(k, cx, cy);
    circle_fill(cx, cy, 9, LINE_C);
    circle_fill(cx, cy, 8, k->hair);
    ellipse_fill(cx, cy + 2, 7, 6, SKIN);
    /* fringe: three tips dipping into the forehead, plus side locks */
    rect(cx - 7, cy - 4, 15, 2, k->hair);
    tri_fill(cx - 7, cy - 3, cx - 3, cy - 3, cx - 6, cy + 1, k->hair);
    tri_fill(cx - 3, cy - 3, cx + 2, cy - 3, cx - 1, cy, k->hair);
    tri_fill(cx + 2, cy - 3, cx + 7, cy - 3, cx + 6, cy + 1, k->hair);
    if (k->style != HAIR_SHORT) {
        rect(cx - 8, cy - 1, 2, 7, k->hair);
        rect(cx + 7, cy - 1, 2, 7, k->hair);
    }
    if (look == LK_SUZU || look == LK_NIA || look == LK_KURO) {   /* calico patch, blue streak, grey temples */
        rect(cx + 2, cy - 6, 4, 3, k->hair_dark);
        pset(cx - 6, cy - 2, k->hair_dark);
    }
    hline(cx - 4, cy - 6, 3, blend(k->hair, WHITE, 110));
    if (k->mask) {
        ellipse_fill(cx - 3, cy + 2, 3, 2, k->hair_dark);
        ellipse_fill(cx + 3, cy + 2, 3, 2, k->hair_dark);
    }
    eyes(k, cx, cy + 2, expr);
    if (!k->mask) {
        rect(cx - 6, cy + 4, 2, 1, BLUSH);
        rect(cx + 5, cy + 4, 2, 1, BLUSH);
    }
    mouth(cx, cy + 5, expr);
    if (k->whiskers) {
        line(cx - 10, cy + 3, cx - 7, cy + 4, LINE_C);
        line(cx - 10, cy + 6, cx - 7, cy + 5, LINE_C);
        line(cx + 7, cy + 4, cx + 10, cy + 3, LINE_C);
        line(cx + 7, cy + 5, cx + 10, cy + 6, LINE_C);
    }
    if (k->glasses) {
        rect_line(cx - 6, cy, 5, 5, LINE_C);
        rect_line(cx + 2, cy, 5, 5, LINE_C);
        pset(cx, cy + 1, LINE_C);
        pset(cx + 1, cy + 1, LINE_C);
    }
    if (look == LK_MAKO) {                                        /* the cyan gem on her headband */
        hline(cx - 6, cy - 7, 13, LINE_C);
        rect(cx - 1, cy - 8, 3, 3, k->accent);
    }
    if (look == LK_SUZU)                                          /* a flower behind her ear */
        for (int p = 0; p < 4; p++)
            circle_fill(cx - 8 + (p % 2) * 3, cy - 6 + (p / 2) * 3, 1, k->accent);
    if (k->ears == EARS_DOG)
        ears(k, cx, cy);
    if (look == LK_KURO) {                                        /* the old fisherman's straw hat */
        ellipse_fill(cx, cy - 7, 12, 3, LINE_C);
        ellipse_fill(cx, cy - 7, 11, 2, k->accent);
        ellipse_fill(cx, cy - 10, 6, 4, LINE_C);
        ellipse_fill(cx, cy - 10, 5, 3, k->accent);
        hline(cx - 5, cy - 8, 11, RGB(184, 60, 60));
    }
    if (look == LK_PONTA) {                                       /* the leaf on the tanuki's head */
        tri_fill(cx - 3, cy - 8, cx + 3, cy - 9, cx + 1, cy - 14, k->accent);
        pset(cx, cy - 8, RGB(60, 120, 60));
    }
}

static void torso(const ChibiLook *k, int look, int cx, int top, int h)
{
    round_box(cx - 7, top, 14, h, LINE_C, k->outfit);
    hline(cx - 5, top + h - 2, 10, k->outfit_dark);
    if (k->apron) {
        rect(cx - 4, top + 3, 8, h - 3, k->accent);
        hline(cx - 4, top + 3, 8, blend(k->accent, WHITE, 80));
    }
    if (look == LK_WHISKERS) {                                    /* bow tie */
        tri_fill(cx - 3, top + 1, cx - 3, top + 4, cx, top + 2, k->accent);
        tri_fill(cx + 3, top + 1, cx + 3, top + 4, cx, top + 2, k->accent);
    }
}

void chibi_bust(int look, int cx, int cy, int expr)
{
    const ChibiLook *k = &chibi_looks[look];
    torso(k, look, cx, cy + 8, 12);
    chibi_head(look, cx, cy, expr);
}

static void tail(const ChibiLook *k, int cx, int cy, int dir, int wag)
{
    switch (k->ears) {
    case EARS_FOX:
        ellipse_fill(cx + dir * 10, cy + 14 + wag, 5, 7, LINE_C);
        ellipse_fill(cx + dir * 10, cy + 14 + wag, 4, 6, k->hair);
        ellipse_fill(cx + dir * 11, cy + 9 + wag, 2, 2, WHITE);
        break;
    case EARS_BUNNY:
        circle_fill(cx + dir * 7, cy + 18, 3, LINE_C);
        circle_fill(cx + dir * 7, cy + 18, 2, WHITE);
        break;
    case EARS_TANUKI:
        ellipse_fill(cx + dir * 10, cy + 16, 4, 6, LINE_C);
        ellipse_fill(cx + dir * 10, cy + 16, 3, 5, k->hair);
        hline(cx + dir * 10 - 3, cy + 14, 7, k->hair_dark);
        hline(cx + dir * 10 - 3, cy + 18, 7, k->hair_dark);
        break;
    default:                                                   /* cat and dog: a curling tail */
        for (int i = 0; i < 6; i++) {
            int x = cx + dir * (6 + i), y = cy + 19 - i * 2 - (i > 3 ? (wag & 1) : 0);
            rect(x - 1, y - 1, 3, 3, LINE_C);
        }
        for (int i = 0; i < 6; i++) {
            int x = cx + dir * (6 + i), y = cy + 19 - i * 2 - (i > 3 ? (wag & 1) : 0);
            pset(x, y, k->ears == EARS_DOG ? k->hair_dark : k->hair);
            pset(x, y - 1, k->ears == EARS_DOG ? k->hair_dark : k->hair);
        }
        break;
    }
}

void chibi(int look, int cx, int cy, int expr, int walk, int flip)
{
    const ChibiLook *k = &chibi_looks[look];
    int phase = walk & 3, bob = (phase == 1 || phase == 3) ? -1 : 0;
    int dir = flip ? 1 : -1;
    tail(k, cx, cy + bob, dir, (int)(frame_count / 12) % 2);
    /* legs */
    int lift_l = phase == 1 ? 2 : 0, lift_r = phase == 3 ? 2 : 0;
    rect(cx - 4, cy + 17 - lift_l, 3, 6, LINE_C);
    rect(cx + 1, cy + 17 - lift_r, 3, 6, LINE_C);
    rect(cx - 3, cy + 18 - lift_l, 1, 4, k->outfit_dark);
    rect(cx + 2, cy + 18 - lift_r, 1, 4, k->outfit_dark);
    torso(k, look, cx, cy + 8 + bob, 11);
    /* arms swing a little while walking */
    int sw = phase == 1 ? 1 : phase == 3 ? -1 : 0;
    rect(cx - 9, cy + 10 + bob + sw, 3, 6, LINE_C);
    rect(cx + 6, cy + 10 + bob - sw, 3, 6, LINE_C);
    rect(cx - 8, cy + 11 + bob + sw, 1, 3, k->outfit);
    rect(cx + 7, cy + 11 + bob - sw, 1, 3, k->outfit);
    pset(cx - 8, cy + 14 + bob + sw, SKIN);
    pset(cx + 7, cy + 14 + bob - sw, SKIN);
    chibi_head(look, cx, cy + bob, expr);
}
