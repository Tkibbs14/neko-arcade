/* Animated dating-sim portraits: the layered bust at 2x or more plus the tells the character cards describe:
 * ears that flick, swivel, perk, flatten or curl (each ear on its own), a tail that sways, flicks, curls, tucks,
 * wraps or goes completely still, hands for the cards' gestures (Shio's glasses and wrist, Mako's chain, Nia's
 * fringe), and where she stands. The scene sets cues per line; the portrait animates toward them. */
#pragma once
#include "engine.h"

enum { EAR_UP, EAR_PERK, EAR_FLICK, EAR_BACK, EAR_DROOP, EAR_FLAT, EAR_CURL, EAR_POSES };
/* what the ears do over time */
enum { EM_UP, EM_PERK, EM_FLAT, EM_BACK, EM_CURL, EM_DROOP, EM_STILL, EM_SWIVEL, EM_COUNT };
enum { TM_SWAY, TM_FLICK, TM_STILL, TM_CURL, TM_TUCK, TM_WRAP, TM_LOW, TM_WAG, TM_PUFF, TM_COUNT };
enum { PO_MID, PO_FAR, PO_CLOSE, PO_LEAN, PO_AWAY, PO_COUNT };
enum { GE_NONE, GE_GLASSES, GE_WRIST, GE_CHAIN, GE_FRINGE, GE_COUNT };
#define OUTFITS 3

typedef struct {
    int who, outfit, expr, aside, blush, talking;
    int ear_mode, tail_mode, pose, gesture;
    /* animation state */
    int x, y, s;                         /* placement: screen x, y and scale, all q8 (s 512 = 2x) */
    int tx, ty, ts;
    int ear_flick[2], ear_next, ear_pop, ear_side;
    int tail_t, tail_flick, tail_puff, tip_x, tip_y;   /* tip in bust pixels, q8 */
    int gest_t, gest_life, gest_draw;    /* hand slide (0..8), frames left, which hand is on screen */
    int blink, blink_next, t;
} Portrait;

void portrait_init(Portrait *p, int who, int outfit);
/* Cues for the line being said; -1 keeps the current value. */
void portrait_cue(Portrait *p, int expr, int ear_mode, int tail_mode, int pose, int gesture, int blush, int aside);
void portrait_flick_ear(Portrait *p);   /* one quick ear flick now */
void portrait_flick_tail(Portrait *p);
void portrait_snap(Portrait *p);        /* jump straight to the target placement (a scene begins) */
void portrait_update(Portrait *p);
void portrait_draw(const Portrait *p);
const char *portrait_outfit_name(int who, int outfit);
/* The same character standing, head to toe, in her outfit (title and pick screens): 56x128 at X, Y (top left), scale s
 * (256 = 1x); the portrait's blinks, ears and tail animate it. dim 0..256 darkens her into the shadow. */
#define FIG_W 56
#define FIG_H 128
void figure_draw(const Portrait *p, int X, int Y, int s, int dim);
