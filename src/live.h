/* A character line made on the device. The composer picks an opening sentence from the teacher's lines for this
 * speaker and situation, and NekoLM scores a few possible continuations (rests of other teacher lines) and keeps
 * the most natural one: new combinations every time, built from sentences that make sense. Where no composition
 * fits, a whole teacher line is used. Jobs wait in several places at once; the model works on one at a time,
 * and a job whose work another caller took over simply starts again. */
#pragma once
#include "engine.h"
#include "compose.h"

enum { LIVE_PENDING, LIVE_RUNNING, LIVE_DONE };
#ifndef LIVE_CANDS
#define LIVE_CANDS 3               /* continuations scored per line (tools/rank_eval.c builds with 6) */
#endif

typedef struct {
    i16 spk, sit;
    const char *key;           /* placeholder name, e.g. "item", or 0 */
    char value[28];            /* what the placeholder becomes */
    char out[128];             /* the finished line */
    u8 state;
    u32 ticket;
    /* the composer's work in progress */
    const CPool *pool;
    i16 op, cand[LIVE_CANDS], ncand, stage, ci, ti, prefix_pos;
    i32 score[LIVE_CANDS];
} LiveJob;

extern int live_ban_he;        /* skip sentences saying he/him/his (the detective's suspects are women) */
extern int live_require_approved;   /* 1: a situation without approved pairs gets whole teacher lines (default) */
extern u32 live_stat_composed, live_stat_whole;   /* lines made each way, for the playtest log */

void live_init(LiveJob *j, int spk, int sit, const char *key, const char *value);
/* Work on the job within this frame's model budget (at most max_slices slices); returns 1 once j->out is final. */
int live_step(LiveJob *j, int max_us, int max_slices);
