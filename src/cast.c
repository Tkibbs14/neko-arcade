/* Character looks and bust composition. Each layer of art/bust.txt is drawn with a ramp built from
 * the character's look, so one face, ear and eye drawing serves the whole cast. */
#include "cast.h"

typedef struct {
    u16 outline, skin, skin_sh, blush, mouth;
    u16 hair[3], hair_acc;              /* light, base, shadow; streaks */
    u16 ear[3], ear_tip;                /* fur light, base, shadow; tip */
    u16 eye_dark, iris;
    u16 outfit[3], outfit_acc;
    u16 acc_main, acc_mark;             /* accessory colour, accessory secondary */
    int hair_spr, outfit_spr, acc_spr;  /* acc_spr -1 = none */
} Look;

const CastInfo cast[CH_COUNT] = {
    { "Nia", -5, RGB(154, 124, 224) },
    { "Mako", 3, RGB(255, 110, 190) },
    { "Shio", 0, RGB(122, 224, 192) },
    { "Mocha", 5, RGB(255, 176, 100) },
};

static const Look looks[CH_COUNT] = {
    [CH_NIA] = {
        RGB(30, 24, 48), RGB(255, 240, 230), RGB(243, 195, 179), RGB(255, 154, 180), RGB(184, 66, 110),
        { RGB(90, 120, 200), RGB(38, 40, 64), RGB(20, 20, 42) }, RGB(106, 168, 255),
        { RGB(70, 84, 140), RGB(38, 40, 64), RGB(20, 20, 42) }, RGB(20, 20, 42),
        RGB(58, 36, 102), RGB(154, 124, 224),
        { RGB(201, 197, 218), RGB(141, 136, 166), RGB(97, 92, 124) }, RGB(255, 255, 255),
        RGB(106, 168, 255), RGB(106, 168, 255),
        SPR_BUST_HAIR_NIA, SPR_BUST_OUTFIT_HOODIE, SPR_BUST_ACC_CLIP,
    },
    [CH_MAKO] = {
        RGB(58, 24, 48), RGB(255, 240, 232), RGB(245, 196, 188), RGB(255, 140, 180), RGB(184, 50, 100),
        { RGB(255, 138, 216), RGB(214, 64, 158), RGB(142, 32, 104) }, RGB(255, 190, 235),
        { RGB(255, 138, 216), RGB(214, 64, 158), RGB(142, 32, 104) }, RGB(110, 24, 80),
        RGB(58, 30, 90), RGB(176, 124, 240),
        { RGB(255, 236, 246), RGB(74, 58, 94), RGB(46, 36, 64) }, RGB(255, 208, 96),
        RGB(106, 224, 240), RGB(255, 255, 255),
        SPR_BUST_HAIR_MAKO, SPR_BUST_OUTFIT_JACKET, SPR_BUST_ACC_HEADBAND,
    },
    [CH_SHIO] = {
        RGB(30, 58, 52), RGB(255, 242, 234), RGB(240, 200, 186), RGB(255, 160, 184), RGB(180, 80, 100),
        { RGB(212, 255, 240), RGB(122, 224, 192), RGB(63, 168, 140) }, RGB(212, 255, 240),
        { RGB(255, 255, 255), RGB(236, 244, 242), RGB(184, 200, 196) }, RGB(122, 224, 192),
        RGB(30, 58, 74), RGB(90, 154, 184),
        { RGB(244, 234, 214), RGB(216, 198, 160), RGB(168, 146, 108) }, RGB(255, 255, 255),
        RGB(42, 42, 58), RGB(200, 192, 204),
        SPR_BUST_HAIR_SHIO, SPR_BUST_OUTFIT_CARDIGAN, SPR_BUST_ACC_GLASSES,
    },
    [CH_MOCHA] = {
        RGB(70, 40, 24), RGB(255, 240, 228), RGB(240, 196, 170), RGB(255, 150, 160), RGB(190, 70, 80),
        { RGB(240, 196, 138), RGB(200, 132, 74), RGB(138, 84, 48) }, RGB(255, 222, 170),
        { RGB(240, 196, 138), RGB(200, 132, 74), RGB(138, 84, 48) }, RGB(90, 52, 32),
        RGB(90, 46, 16), RGB(232, 160, 48),
        { RGB(168, 216, 192), RGB(106, 184, 152), RGB(63, 136, 112) }, RGB(255, 122, 168),
        RGB(255, 122, 168), RGB(255, 122, 168),
        SPR_BUST_HAIR_MOCHA, SPR_BUST_OUTFIT_APRON, SPR_BUST_ACC_BANDANA,
    },
};

/* Ramp slots (art/bust.txt): 1 k outline, 2 s skin, 3 d skin shadow, 4-6 h/H/j hair or fur,
 * 7 e eye dark, 8 i iris, 9 w white, 10-12 o/O/q outfit, 13 p pink, 14 m mouth, 15 a accent. */
enum { LAYER_BODY, LAYER_HAIR, LAYER_EAR, LAYER_ACC };

static void build_ramp(const Look *L, int layer, u16 *r)
{
    r[0] = 0;
    r[1] = L->outline; r[2] = L->skin; r[3] = L->skin_sh;
    r[4] = L->hair[0]; r[5] = L->hair[1]; r[6] = L->hair[2];
    r[7] = L->eye_dark; r[8] = L->iris; r[9] = RGB(255, 255, 255);
    r[10] = L->outfit[0]; r[11] = L->outfit[1]; r[12] = L->outfit[2];
    r[13] = L->blush; r[14] = L->mouth; r[15] = L->outfit_acc;
    if (layer == LAYER_HAIR)
        r[15] = L->hair_acc;
    if (layer == LAYER_EAR) {
        r[4] = L->ear[0]; r[5] = L->ear[1]; r[6] = L->ear[2];
        r[15] = L->ear_tip;
    }
    if (layer == LAYER_ACC) {
        r[15] = L->acc_main;
        r[13] = L->acc_mark;
        r[1] = L->outline;
    }
}

typedef struct { u8 eyes, mouth, blush, ears_flat; } Expr;

static const Expr exprs[EX_COUNT] = {
    [EX_NEUTRAL]   = { SPR_BUST_EYES_NEUTRAL - SPR_BUST_EYES_NEUTRAL, 2, 0, 0 },
    [EX_HAPPY]     = { SPR_BUST_EYES_HAPPY - SPR_BUST_EYES_NEUTRAL, 1, 1, 0 },
    [EX_ANNOYED]   = { SPR_BUST_EYES_HALF - SPR_BUST_EYES_NEUTRAL, 3, 0, 1 },
    [EX_SURPRISED] = { SPR_BUST_EYES_WIDE - SPR_BUST_EYES_NEUTRAL, 4, 0, 0 },
    [EX_SMUG]      = { SPR_BUST_EYES_HALF - SPR_BUST_EYES_NEUTRAL, 5, 0, 0 },
    [EX_FLUSTERED] = { SPR_BUST_EYES_NEUTRAL - SPR_BUST_EYES_NEUTRAL, 3, 1, 1 },
    [EX_SAD]       = { SPR_BUST_EYES_CLOSED - SPR_BUST_EYES_NEUTRAL, 3, 0, 1 },
    [EX_SPARKLE]   = { SPR_BUST_EYES_SPARKLE - SPR_BUST_EYES_NEUTRAL, 0, 1, 0 },
    [EX_TIRED]     = { SPR_BUST_EYES_HALF - SPR_BUST_EYES_NEUTRAL, 2, 0, 0 },
};

static const int mouth_sprites[7] = {
    SPR_BUST_MOUTH_CAT, SPR_BUST_MOUTH_SMILE, SPR_BUST_MOUTH_FLAT, SPR_BUST_MOUTH_POUT,
    SPR_BUST_MOUTH_OPEN, SPR_BUST_MOUTH_SMIRK, SPR_BUST_MOUTH_FANG,
};

static void bust(int who, int ex, int x, int y, int s, int flags)
{
    const Look *L = &looks[who];
    const Expr *E = &exprs[ex < 0 || ex >= EX_COUNT ? EX_NEUTRAL : ex];
    u16 body[16], hair[16], ear[16], acc[16];
    build_ramp(L, LAYER_BODY, body);
    build_ramp(L, LAYER_HAIR, hair);
    build_ramp(L, LAYER_EAR, ear);
    build_ramp(L, LAYER_ACC, acc);
    /* blink for 6 frames every few seconds (offset per character so they do not blink together) */
    int eyes = SPR_BUST_EYES_NEUTRAL + E->eyes;
    u32 t = (frame_count + (u32)who * 97u) % 211u;
    if (t < 6 && (eyes == SPR_BUST_EYES_NEUTRAL || eyes == SPR_BUST_EYES_SPARKLE || eyes == SPR_BUST_EYES_WIDE))
        eyes = SPR_BUST_EYES_CLOSED;
    /* sub-sprites are placed relative to the 64 px bust; mirror their x when flipped */
    #define PX(ox, w) (flags & FLIP_X ? x + (64 - (ox) - (w)) * s : x + (ox) * s)
    spr_scaled(L->outfit_spr, x, y, s, flags, body);
    spr_scaled(SPR_BUST_FACE, x, y, s, flags, body);
    spr_scaled(eyes, PX(16, spr_w(eyes)), y + 32 * s, s, flags, body);
    spr_scaled(mouth_sprites[E->mouth], PX(27, 11), y + 46 * s, s, flags, body);
    if (E->blush)
        spr_scaled(SPR_BUST_BLUSH, PX(16, 32), y + 44 * s, s, flags, body);
    spr_scaled(L->hair_spr, x, y, s, flags, hair);
    spr_scaled(E->ears_flat ? SPR_BUST_EARS_FLAT : SPR_BUST_EARS_UP, x, y, s, flags, ear);
    if (L->acc_spr >= 0)
        spr_scaled(L->acc_spr, x, y, s, flags, acc);
    #undef PX
}

void draw_bust(int who, int expr, int x, int y, int scale) { bust(who, expr, x, y, scale, 0); }
void draw_bust_flipped(int who, int expr, int x, int y, int scale) { bust(who, expr, x, y, scale, FLIP_X); }
