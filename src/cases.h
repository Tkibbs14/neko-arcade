/* Whisker Detective cases: written by the teacher model, accepted only if the logic checker passes
 * (tools/cases_gen.py), then reviewed by hand (tools/cases_fix.py). */
#pragma once
#include "engine.h"

/* Where a clue sits in the kitchen (baked from the words in its description). */
enum {
    SPOT_FRIDGE, SPOT_PANTRY, SPOT_COUNTER, SPOT_SINK, SPOT_STOVE, SPOT_TABLE, SPOT_CHAIR, SPOT_COUCH,
    SPOT_TRASH, SPOT_FLOOR, SPOT_DOOR, SPOT_COUNT
};
enum { PLACE_APARTMENT, PLACE_CAFE };

typedef struct { const char *name, *desc; u8 spot; } Clue;
typedef struct { i8 who; const char *text; i8 contra; } Statement;   /* contra: clue index that disproves it, or -1 */
typedef struct {
    const char *title, *intro;
    i8 culprit;                 /* cast index */
    u8 place;
    const Clue *clues;
    int nclues;
    const Statement *st;
    int nst;
    const char *confession, *wrapup;
} Case;

extern const Case cases[];
extern const int case_count;
