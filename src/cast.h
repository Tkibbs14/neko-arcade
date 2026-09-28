/* The cast: Tyler's published characters (Nia, Mako, Shio) and new originals, drawn as layered busts. */
#pragma once
#include "engine.h"

enum { CH_NIA, CH_MAKO, CH_SHIO, CH_MOCHA, CH_COUNT };

enum {
    EX_NEUTRAL, EX_HAPPY, EX_ANNOYED, EX_SURPRISED, EX_SMUG, EX_FLUSTERED, EX_SAD, EX_SPARKLE, EX_TIRED,
    EX_COUNT
};

typedef struct {
    const char *name;
    int voice;                 /* dialogue blip pitch, semitones from A4 */
    u16 name_color;
} CastInfo;

extern const CastInfo cast[CH_COUNT];

/* Colour ramps and sprites for building portraits elsewhere (the dating sim's animated portraits). */
enum { CL_BODY, CL_HAIR, CL_EAR, CL_ACC };
void cast_ramp(int who, int layer, u16 *r);
int cast_hair_sprite(int who);
int cast_acc_sprite(int who);                 /* -1: none */
void cast_fur(int who, u16 *light, u16 *base, u16 *shadow, u16 *tip, u16 *outline);   /* ears and tail */

/* Draw a bust (64x64 art) at x,y with an integer scale; blinks on its own. */
void draw_bust(int who, int expr, int x, int y, int scale);
/* Same, facing the other way. */
void draw_bust_flipped(int who, int expr, int x, int y, int scale);
