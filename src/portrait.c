#include "portrait.h"
#include "cast.h"

typedef struct { int spr; u16 o, O, q; const char *name; } Outfit;

/* PG outfits: the cards' everyday clothes first, then two unlockable ones each (bare shoulders at most). */
static const Outfit outfits[3][OUTFITS] = {
    [CH_NIA] = {
        { SPR_BUST_OUTFIT_HOODIE, RGB(201, 197, 218), RGB(141, 136, 166), RGB(97, 92, 124), "Worn-in hoodie" },
        { SPR_BUST_OUTFIT_SWEATER_OFF, RGB(238, 228, 248), RGB(196, 178, 226), RGB(150, 130, 190), "Slouchy sweater" },
        { SPR_BUST_OUTFIT_TANK_HOODIE, RGB(250, 250, 252), RGB(84, 90, 128), RGB(46, 48, 74), "Open hoodie" },
    },
    [CH_MAKO] = {
        { SPR_BUST_OUTFIT_JACKET, RGB(255, 236, 246), RGB(74, 58, 94), RGB(46, 36, 64), "Cropped jacket" },
        { SPR_BUST_OUTFIT_CROP_SPORT, RGB(255, 244, 250), RGB(222, 70, 170), RGB(142, 32, 104), "Game-day top" },
        { SPR_BUST_OUTFIT_DRESS, RGB(176, 128, 244), RGB(96, 54, 156), RGB(56, 32, 96), "Going-out dress" },
    },
    [CH_SHIO] = {
        { SPR_BUST_OUTFIT_CARDIGAN, RGB(244, 234, 214), RGB(216, 198, 160), RGB(168, 146, 108), "Oversized cardigan" },
        { SPR_BUST_OUTFIT_BLOUSE, RGB(234, 244, 255), RGB(172, 200, 232), RGB(112, 140, 180), "Pressed blouse" },
        { SPR_BUST_OUTFIT_SUNDRESS, RGB(228, 250, 242), RGB(122, 224, 192), RGB(63, 168, 140), "Mint sundress" },
    },
};

const char *portrait_outfit_name(int who, int outfit) { return outfits[who][outfit].name; }

/* expressions (as the hub busts): eyes sprite offset, mouth, blush */
enum { EY_NEUTRAL, EY_HAPPY, EY_HALF, EY_WIDE, EY_CLOSED, EY_SPARKLE, EY_ASIDE };
enum { MO_CAT, MO_SMILE, MO_FLAT, MO_POUT, MO_OPEN, MO_SMIRK, MO_FANG };
static const u8 ex_eyes[EX_COUNT] = { EY_NEUTRAL, EY_HAPPY, EY_HALF, EY_WIDE, EY_HALF, EY_NEUTRAL, EY_CLOSED,
                                      EY_SPARKLE, EY_HALF };
static const u8 ex_mouth[EX_COUNT] = { MO_FLAT, MO_SMILE, MO_POUT, MO_OPEN, MO_SMIRK, MO_POUT, MO_POUT, MO_CAT, MO_FLAT };
static const u8 ex_blush[EX_COUNT] = { 0, 1, 0, 0, 0, 1, 0, 1, 0 };

/* where she stands: mid, far (doorway), close, leaning in, turned away; x, y on screen and scale (512 = 2x) */
static const int pose_x[PO_COUNT] = { 150, 194, 138, 132, 176 };
static const int pose_y[PO_COUNT] = { 14, 22, 8, 16, 16 };
static const int pose_s[PO_COUNT] = { 512, 464, 576, 608, 512 };

void portrait_init(Portrait *p, int who, int outfit)
{
    mem_set(p, 0, sizeof *p);
    p->who = who;
    p->outfit = outfit < 0 || outfit >= OUTFITS ? 0 : outfit;
    p->expr = EX_NEUTRAL;
    p->ear_mode = EM_UP;
    p->tail_mode = TM_SWAY;
    p->pose = PO_MID;
    p->tip_x = 51 << 8;
    p->tip_y = 20 << 8;
    p->blink_next = 90 + who * 37;
    p->ear_next = 120 + who * 50;
    p->tx = pose_x[PO_MID] << 8; p->ty = pose_y[PO_MID] << 8; p->ts = pose_s[PO_MID];
    portrait_snap(p);
}

void portrait_snap(Portrait *p)
{
    p->x = p->tx; p->y = p->ty; p->s = p->ts;
}

void portrait_flick_ear(Portrait *p)
{
    p->ear_side ^= 1;
    p->ear_flick[p->ear_side] = 7;
}

void portrait_flick_tail(Portrait *p) { p->tail_flick = 14; }

void portrait_cue(Portrait *p, int expr, int ear_mode, int tail_mode, int pose, int gesture, int blush, int aside)
{
    if (expr >= 0 && expr < EX_COUNT) p->expr = expr;
    if (ear_mode >= 0 && ear_mode < EM_COUNT) {
        if (ear_mode != p->ear_mode && (ear_mode == EM_PERK || ear_mode == EM_UP))
            p->ear_pop = 5;                              /* ears pop up when they come alert */
        if (ear_mode == EM_SWIVEL)
            p->ear_next = 4;
        p->ear_mode = ear_mode;
    }
    if (tail_mode >= 0 && tail_mode < TM_COUNT) {
        if (tail_mode == TM_FLICK) p->tail_flick = 14;
        if (tail_mode == TM_PUFF) p->tail_puff = 48;
        p->tail_mode = tail_mode;
    }
    if (pose >= 0 && pose < PO_COUNT) {
        p->pose = pose;
        p->tx = pose_x[pose] << 8; p->ty = pose_y[pose] << 8; p->ts = pose_s[pose];
    }
    if (gesture >= 0 && gesture < GE_COUNT) {
        p->gesture = gesture;
        if (gesture != GE_NONE) {
            p->gest_draw = gesture;
            p->gest_life = (gesture == GE_GLASSES || gesture == GE_FRINGE) ? 60 : 1 << 20;   /* one-shot or held */
        } else {
            p->gest_life = 0;
        }
    }
    if (blush >= 0) p->blush = blush;
    if (aside >= 0) p->aside = aside;
}

static int ease(int cur, int target, int div)
{
    int d = target - cur;
    if (d == 0) return cur;
    int step = d / div;
    if (step == 0) step = d > 0 ? 1 : -1;
    return cur + step;
}

void portrait_update(Portrait *p)
{
    p->t++;
    p->x = ease(p->x, p->tx, 6);
    p->y = ease(p->y, p->ty, 6);
    p->s = ease(p->s, p->ts, 6);
    /* blinking goes on even when she freezes: people still blink */
    if (p->blink > 0) p->blink--;
    else if (--p->blink_next <= 0) { p->blink = 5; p->blink_next = 150 + (int)(rnd() % 130); }
    /* ears: idle twitches, or the alternating swivel */
    for (int s = 0; s < 2; s++)
        if (p->ear_flick[s] > 0) p->ear_flick[s]--;
    if (p->ear_pop > 0) p->ear_pop--;
    if (p->ear_mode != EM_STILL && --p->ear_next <= 0) {
        if (p->ear_mode == EM_SWIVEL) {
            p->ear_side ^= 1;
            p->ear_flick[p->ear_side] = 16;
            p->ear_next = 22 + (int)(rnd() % 22);
        } else {
            p->ear_side = (int)(rnd() & 1);
            p->ear_flick[p->ear_side] = 6;
            p->ear_next = (p->ear_mode == EM_CURL ? 300 : 150) + (int)(rnd() % 260);
        }
    }
    /* hands slide in, hold (a one-shot gesture times out), slide away */
    if (p->gest_life > 0) {
        p->gest_life--;
        if (p->gest_t < 8) p->gest_t++;
    } else if (p->gest_t > 0) {
        p->gest_t--;
    } else {
        p->gest_draw = GE_NONE;
    }
    /* tail */
    if (p->tail_flick > 0) p->tail_flick--;
    if (p->tail_puff > 0) p->tail_puff--;
    if (p->tail_mode == TM_STILL)
        return;                                       /* completely still: the tip stays where it was */
    p->tail_t += p->tail_mode == TM_WAG ? 9 : p->tail_mode == TM_LOW ? 1 : 3;
    int a = p->tail_t / 4, tx, ty;
    switch (p->tail_mode) {
    case TM_WAG:  tx = (51 << 8) + isin(a * 3) * 7; ty = (19 << 8) + icos(a * 5) * 3; break;
    case TM_CURL: tx = (49 << 8) + isin(a) * 2; ty = 24 << 8; break;
    case TM_LOW:  tx = (63 << 8) + isin(a) * 1; ty = 53 << 8; break;
    case TM_TUCK: tx = 61 << 8; ty = 80 << 8; break;
    case TM_WRAP: tx = (15 << 8) + isin(a) * 1; ty = 60 << 8; break;
    default:      tx = (51 << 8) + isin(a) * 3; ty = (19 << 8) + icos(a * 3 / 2) * 2; break;
    }
    if (p->tail_flick > 0) {                          /* a quick flick of the tip, out and back */
        int e = p->tail_flick > 7 ? 14 - p->tail_flick : p->tail_flick;
        tx += e * 330;
        ty -= e * 90;
    }
    p->tip_x = ease(p->tip_x, tx, 4);
    p->tip_y = ease(p->tip_y, ty, 4);
}

/* ---------------------------------------------------------------- drawing */

static void bezier(const int *cx, const int *cy, int t, int *x, int *y)   /* t 0..256, points q8 */
{
    i64 u = 256 - t, a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, d = (i64)t * t * t;
    *x = (int)((a * cx[0] + b * cx[1] + c * cx[2] + d * cx[3]) >> 24);
    *y = (int)((a * cy[0] + b * cy[1] + c * cy[2] + d * cy[3]) >> 24);
}

#define TAIL_N 44
/* Samples a tail along its Bezier (cx, cy: source pixels, q8), the tip curling in on itself for TM_CURL, and draws it
 * as one furry tube at X, Y and scale s. half: the full-body figure, whose tail is half the portrait's. fur: light,
 * base, shadow, tip, outline. */
static void tail_tube(const Portrait *p, const int *cx, const int *cy, int half, int X, int Y, int s, const u16 *fur)
{
    int n = TAIL_N + (p->tail_mode == TM_CURL ? 12 : 0);
    int xs[TAIL_N + 12], ys[TAIL_N + 12], rs[TAIL_N + 12];
    int puff = 256 + p->tail_puff * 3;                /* puffed up, then settling */
    int curl = half ? 3 : 6;                          /* the curl's radius, in half pixels */
    for (int i = 0; i < n; i++) {
        int bx, by, t = i < TAIL_N ? i * 256 / (TAIL_N - 1) : 256;
        if (i < TAIL_N) {
            bezier(cx, cy, t, &bx, &by);
        } else {                                      /* the tip curls in on itself */
            int k = i - TAIL_N + 1, ang = 64 + k * 10;
            bx = cx[3] - (curl << 7) + icos(ang) * curl / 2;
            by = cy[3] - isin(ang) * curl / 2 + (1 << 8);
        }
        int r = (4 << 8) + 110 - (t * 7 >> 2) - (i >= TAIL_N ? (i - TAIL_N) * 14 : 0);
        r = (r * puff >> 8) >> half;
        xs[i] = X + (int)(((i64)bx * s) >> 16);
        ys[i] = Y + (int)(((i64)by * s) >> 16);
        rs[i] = imax(1, (int)(((i64)r * s) >> 16));
    }
    int tip_from = n * 4 / 5;
    for (int i = 0; i < n; i++)
        circle_fill(xs[i], ys[i], rs[i] + 1, fur[4]);
    for (int i = 0; i < n; i++)
        circle_fill(xs[i], ys[i], rs[i], i >= tip_from ? fur[3] : fur[1]);
    for (int i = 0; i < n; i++)
        if (rs[i] >= 3)
            circle_fill(xs[i] + rs[i] / 3, ys[i] + rs[i] / 4, rs[i] / 2, i >= tip_from ? blend(fur[3], fur[4], 70) : fur[2]);
    for (int i = 0; i < n; i++)
        if (rs[i] >= 3)
            circle_fill(xs[i] - rs[i] / 3, ys[i] - rs[i] / 4, rs[i] / 3, i >= tip_from ? blend(fur[3], C_WHITE, 90) : fur[0]);
}

static void draw_tail(const Portrait *p, int dy)
{
    if (p->tail_mode == TM_TUCK && p->tip_y > (74 << 8))
        return;
    u16 fur[5];
    cast_fur(p->who, &fur[0], &fur[1], &fur[2], &fur[3], &fur[4]);
    int cx[4], cy[4];
    if (p->tail_mode == TM_WRAP) {                    /* around in front of her */
        cx[0] = 58 << 8; cx[1] = 50 << 8; cx[2] = 30 << 8; cx[3] = p->tip_x;
        cy[0] = 76 << 8; cy[1] = 80 << 8; cy[2] = 74 << 8; cy[3] = p->tip_y;
    } else {                                          /* rising behind her shoulder */
        cx[0] = 58 << 8; cx[1] = 68 << 8; cx[2] = p->tip_x + (10 << 8); cx[3] = p->tip_x;
        cy[0] = 78 << 8; cy[1] = 58 << 8; cy[2] = p->tip_y + (17 << 8); cy[3] = p->tip_y;
    }
    tail_tube(p, cx, cy, 0, p->x >> 8, (p->y >> 8) + dy, p->s, fur);
}

static int ear_pose(const Portrait *p, int side)
{
    static const u8 base[EM_COUNT] = { EAR_UP, EAR_PERK, EAR_FLAT, EAR_BACK, EAR_CURL, EAR_DROOP, EAR_UP, EAR_BACK };
    int b = base[p->ear_mode];
    if (p->ear_flick[side] > 0) {
        if (p->ear_mode == EM_SWIVEL) return EAR_UP;  /* one ear turns forward while the other stays back */
        if (b == EAR_FLAT) return EAR_DROOP;
        if (b == EAR_CURL || b == EAR_DROOP) return EAR_UP;
        return EAR_FLICK;
    }
    return b;
}

void portrait_draw(const Portrait *p)
{
    const Outfit *O = &outfits[p->who][p->outfit];
    u16 body[16], hair[16], ear[16], acc[16];
    cast_ramp(p->who, CL_BODY, body);
    cast_ramp(p->who, CL_HAIR, hair);
    cast_ramp(p->who, CL_EAR, ear);
    cast_ramp(p->who, CL_ACC, acc);
    body[10] = O->o; body[11] = O->O; body[12] = O->q;
    int still = p->tail_mode == TM_STILL && p->ear_mode == EM_STILL;
    int dy = still ? 0 : isin(p->t * 2) / 200;        /* breathing: a pixel of rise and fall */
    int X = p->x >> 8, Y = (p->y >> 8) + dy, s = p->s;
    if (p->tail_mode != TM_WRAP)
        draw_tail(p, dy);
    spr_scaled_q8(O->spr, X, Y, 0, 0, s, 0, body);
    spr_scaled_q8(SPR_BUST_FACE, X, Y, 0, 0, s, 0, body);
    int eyes = p->aside || p->pose == PO_AWAY ? EY_ASIDE : ex_eyes[p->expr];
    if (p->blink > 0 && eyes != EY_HAPPY && eyes != EY_CLOSED)
        eyes = EY_CLOSED;
    spr_scaled_q8(SPR_BUST_EYES_NEUTRAL + eyes, X, Y, 16, 32, s, 0, body);
    int mouth = ex_mouth[p->expr];
    if (p->talking && (p->t / 5) % 2)
        mouth = mouth == MO_OPEN ? MO_SMILE : MO_OPEN;
    spr_scaled_q8(SPR_BUST_MOUTH_CAT + mouth, X, Y, 27, 46, s, 0, body);
    if (p->blush || ex_blush[p->expr])
        spr_scaled_q8(SPR_BUST_BLUSH, X, Y, 16, 44, s, 0, body);
    spr_scaled_q8(cast_hair_sprite(p->who), X, Y, 0, 0, s, 0, hair);
    int pop = p->ear_pop > 0 ? -1 : 0;
    spr_scaled_q8(SPR_BUST_EAR_UP + ear_pose(p, 0), X, Y, 0, pop, s, 0, ear);
    spr_scaled_q8(SPR_BUST_EAR_UP + ear_pose(p, 1), X, Y, 0, pop, s, FLIP_X, ear);
    int acc_spr = cast_acc_sprite(p->who);
    if (acc_spr >= 0) {                               /* the glasses ride up while her finger is on them */
        int lift = p->gest_draw == GE_GLASSES && p->gest_t >= 6 ? -1 : 0;
        spr_scaled_q8(acc_spr, X, Y, 0, lift, s, 0, acc);
    }
    if (p->tail_mode == TM_WRAP)
        draw_tail(p, dy);
    if (p->gest_draw != GE_NONE && p->gest_t > 0) {  /* hands slide up into view */
        int slide = (8 - p->gest_t) * 3;
        spr_scaled_q8(SPR_BUST_HAND_GLASSES + p->gest_draw - 1, X, Y, 0, slide, s, 0, body);
    }
}

/* ---------------------------------------------------------------- full-body figures (title and pick screens) */

static const int fig_body[3][OUTFITS] = {
    [CH_NIA] = { SPR_FIG_BODY_NIA_HOODIE, SPR_FIG_BODY_NIA_SWEATER, SPR_FIG_BODY_NIA_TANK },
    [CH_MAKO] = { SPR_FIG_BODY_MAKO_JACKET, SPR_FIG_BODY_MAKO_SPORT, SPR_FIG_BODY_MAKO_DRESS },
    [CH_SHIO] = { SPR_FIG_BODY_SHIO_CARDIGAN, SPR_FIG_BODY_SHIO_BLOUSE, SPR_FIG_BODY_SHIO_SUNDRESS },
};
static const int fig_hair[3] = { SPR_FIG_HAIR_NIA, SPR_FIG_HAIR_MAKO, SPR_FIG_HAIR_SHIO };
static const int fig_acc[3] = { SPR_FIG_ACC_CLIP, SPR_FIG_ACC_HEADBAND, SPR_FIG_ACC_GLASSES };

static void darken(u16 *c, int n, int dim)
{
    for (int i = 0; i < n; i++)
        c[i] = blend(c[i], RGB(16, 12, 30), dim);
}

/* the portrait's tail tip (its own coordinates) carried over to the figure: the tail leaves her hip and rises beside
 * her, and every tail mode and flick keeps its meaning */
static void figure_tail(const Portrait *p, int X, int Y, int s, const u16 *fur)
{
    if (p->tail_mode == TM_TUCK && p->tip_y > (74 << 8))
        return;
    int tx = (48 << 8) + (p->tip_x - (51 << 8)) * 3 / 5, ty = (34 << 8) + (p->tip_y - (19 << 8)) * 3 / 5;
    int cx[4], cy[4];
    if (p->tail_mode == TM_WRAP) {                    /* around in front of her hips */
        cx[0] = 35 << 8; cx[1] = 43 << 8; cx[2] = 24 << 8; cx[3] = tx;
        cy[0] = 60 << 8; cy[1] = 70 << 8; cy[2] = 70 << 8; cy[3] = ty;
    } else {
        cx[0] = 35 << 8; cx[1] = 47 << 8; cx[2] = tx + (6 << 8); cx[3] = tx;
        cy[0] = 60 << 8; cy[1] = 64 << 8; cy[2] = ty + (14 << 8); cy[3] = ty;
    }
    tail_tube(p, cx, cy, 1, X, Y, s, fur);
}

void figure_draw(const Portrait *p, int X, int Y, int s, int dim)
{
    const Outfit *O = &outfits[p->who][p->outfit];
    u16 body[16], hair[16], ear[16], acc[16], fur[5];
    cast_ramp(p->who, CL_BODY, body);
    cast_ramp(p->who, CL_HAIR, hair);
    cast_ramp(p->who, CL_EAR, ear);
    cast_ramp(p->who, CL_ACC, acc);
    body[10] = O->o; body[11] = O->O; body[12] = O->q;
    cast_fur(p->who, &fur[0], &fur[1], &fur[2], &fur[3], &fur[4]);
    if (dim > 0) {                                    /* standing back in the shadow */
        darken(body, 16, dim); darken(hair, 16, dim); darken(ear, 16, dim); darken(acc, 16, dim);
        darken(fur, 5, dim);
    }
    int still = p->tail_mode == TM_STILL && p->ear_mode == EM_STILL;
    int dy = still ? 0 : isin(p->t * 2) / 200;        /* breathing: her head rises and falls a pixel */
    int hy = Y + (dy * s >> 8);
    if (p->tail_mode != TM_WRAP)
        figure_tail(p, X, Y, s, fur);
    spr_scaled_q8(fig_body[p->who][p->outfit], X, Y, 0, 0, s, 0, body);
    spr_scaled_q8(SPR_FIG_FACE, X, hy, 12, 0, s, 0, body);
    int eyes = p->aside || p->pose == PO_AWAY ? EY_ASIDE : ex_eyes[p->expr];
    if (p->blink > 0 && eyes != EY_HAPPY && eyes != EY_CLOSED)
        eyes = EY_CLOSED;
    spr_scaled_q8(SPR_FIG_EYES_NEUTRAL + eyes, X, hy, 12, 0, s, 0, body);
    int mouth = ex_mouth[p->expr];
    if (p->talking && (p->t / 5) % 2)
        mouth = mouth == MO_OPEN ? MO_SMILE : MO_OPEN;
    spr_scaled_q8(SPR_FIG_MOUTH_CAT + mouth, X, hy, 12, 0, s, 0, body);
    if (p->blush || ex_blush[p->expr])
        spr_scaled_q8(SPR_FIG_BLUSH, X, hy, 12, 0, s, 0, body);
    spr_scaled_q8(fig_hair[p->who], X, hy, 12, 0, s, 0, hair);
    int pop = p->ear_pop > 0 ? -1 : 0;
    spr_scaled_q8(SPR_FIG_EAR_UP + ear_pose(p, 0), X, hy, 12, pop, s, 0, ear);
    spr_scaled_q8(SPR_FIG_EAR_UP + ear_pose(p, 1), X, hy, 12, pop, s, FLIP_X, ear);
    spr_scaled_q8(fig_acc[p->who], X, hy, 12, 0, s, 0, acc);
    if (p->tail_mode == TM_WRAP)
        figure_tail(p, X, Y, s, fur);
}
