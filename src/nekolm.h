/* NekoLM: the tiny distilled language model, integer-only. In the game it ranks candidate lines (see live.c);
 * it can also write lines token by token. All work is time-sliced so the game keeps its frame rate. */
#pragma once
#include "engine.h"
#include "gen/nekolm_data.h"

#define NLM_MAXLEN 88
extern int nlm_topk, nlm_temp_q8;    /* sampling: top-k (at most 16) and 256 / temperature */

/* ---- ownership: whoever holds the current ticket owns the model's state */
u32 nlm_claim(void);                 /* take the model (clears its context) */
u32 nlm_current(void);

/* ---- the forward pass, one slice at a time (a token is nine slices) */
void nlm_feed(int tok);              /* start feeding a token at the current position */
int nlm_slice(void);                 /* run the next slice; 1 once the logits for the fed token are ready */
int nlm_busy(void);                  /* a token is part-way through its slices */
void nlm_pick_kernel(char *report);  /* time the dot-product kernels here, keep the fastest exact one; report it */
const char *nlm_set_kernel(int k);   /* tests: use kernel k (0 plain, 1 madd, 2 split); its name, or 0 */
int nlm_cursor(void);                /* tokens in the context */
void nlm_rewind(int pos);            /* forget everything after pos (to score another continuation) */
i32 nlm_logprob_q8(int tok);         /* log p(tok) under the current logits, in 1/256 nats */

/* ---- time slicing: all model work in a frame shares nlm_frame_budget_us (set by the game loop) */
extern int nlm_frame_budget_us;
extern u32 nlm_frame_us;             /* used so far this frame */
extern u32 nlm_stat_slices, nlm_stat_us, nlm_stat_max_us;   /* totals for the playtest log */
/* Call work(ctx) slice by slice while this frame's budget lasts (at least one slice); returns its last result.
 * Without a platform clock it runs a fixed number of slices. */
int nlm_work_for(int (*work)(void *), void *ctx, int max_slices);

/* ---- writing a line token by token (tests, the quality tools) */
u32 nlm_begin(int spk, int sit, u32 seed);   /* claims the model; SPK_* / SIT_* from gen/nekolm_data.h */
int nlm_run(int max_tokens);         /* up to max_tokens whole tokens; 1 once the line is complete */
int nlm_run_for(int max_us, int max_slices);
int nlm_done(void);
int nlm_complete(void);              /* done, and the model ended the line itself */
const char *nlm_text(void);          /* the line so far (NUL-terminated) */
const char *nlm_line(int spk, int sit, u32 seed);

/* Validate a raw line (before {key} substitution); key may be 0. */
int nlm_ok(const char *s, const char *key);
/* Copy src to dst replacing {key} with value (dst must hold 128 bytes); a value that starts a sentence is
 * capitalised. */
void nlm_fill(char *dst, const char *src, const char *key, const char *value);
/* Test hook: feed tokens and return the Q8 logit of token t after the last one. */
void nlm_reset(void);
const i32 *nlm_forward(int tok);
