/* Chibi figures for the cafe and the village: small procedural heads and bodies drawn from a table of
 * looks (hair, ears, eyes, outfit), so every species and outfit is one row rather than a sprite sheet. */
#pragma once
#include "engine.h"

enum { EARS_CAT, EARS_FOX, EARS_BUNNY, EARS_DOG, EARS_TANUKI };
enum { HAIR_BOB, HAIR_LONG, HAIR_TWIN, HAIR_SHORT, HAIR_PONY };
enum { CE_NORMAL, CE_HAPPY, CE_ANGRY, CE_SLEEPY, CE_SURPRISED, CE_SAD, CE_COUNT };

typedef struct {
    u16 hair, hair_dark, ear, ear_in, eye, outfit, outfit_dark, accent;
    u8 ears, style;
    u8 whiskers, glasses, mask, apron;
} ChibiLook;

enum {
    LK_HIRE, LK_HIRE2, LK_MOCHA, LK_NIA, LK_MAKO, LK_SHIO, LK_KURO, LK_SUZU,
    LK_TSUKI, LK_PIPI, LK_WHISKERS, LK_KITSU, LK_HANA, LK_PONTA, LK_COUNT
};
extern const ChibiLook chibi_looks[LK_COUNT];

/* Head centred at cx,cy: the face is about 18 px wide, ears reach 16 px above the centre. */
void chibi_head(int look, int cx, int cy, int expr);
/* Head and shoulders (for customers behind a counter). */
void chibi_bust(int look, int cx, int cy, int expr);
/* Whole standing figure, head centre at cx,cy, feet near cy+24; walk = animation phase (0 = still). */
void chibi(int look, int cx, int cy, int expr, int walk, int flip);
