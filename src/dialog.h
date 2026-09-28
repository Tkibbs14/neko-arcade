/* Dialogue queue: typewriter text with voice blips; scenes draw the portraits themselves.
 * Live lines are written by NekoLM (see live.h) ahead of the line on screen, so the reader rarely waits;
 * a line still being written shows as a thinking bubble. */
#pragma once
#include "engine.h"

#define DLG_NARRATOR -1
#define DLG_NAMED -2           /* a speaker outside the cast: the line carries its own name and colour */

void dlg_clear(void);
void dlg_push(int who, int expr, const char *text);   /* text must outlive the line (static, or dlg_buf) */
/* A line NekoLM writes live: speaker/situation tags, optional {key} -> value substitution. */
void dlg_push_live(int who, int expr, int spk, int sit, const char *key, const char *value);
void dlg_push_named(const char *name, u16 color, int voice, const char *text);
void dlg_push_live_named(const char *name, u16 color, int voice, int spk, int sit, const char *key,
                         const char *value);
char *dlg_buf(void);                                 /* scratch buffer (256 bytes) for built lines */
int dlg_busy(void);                                  /* a line is showing or queued */
int dlg_just_finished(void);                         /* the queue emptied this frame */
int dlg_wants_model(void);                           /* queued live lines are waiting for NekoLM */
void dlg_update(void);                               /* typewriter + A to advance (either player) */
void dlg_draw(int x, int y, int w, int h);           /* box with name tag and text */
int dlg_who(void);
int dlg_expr(void);
int dlg_typing(void);                                /* still revealing (or still generating) the line */
extern int dlg_nlm_budget;                           /* most model steps per frame for live lines */
