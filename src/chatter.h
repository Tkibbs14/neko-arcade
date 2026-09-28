/* Ambient lines (cafe customers, villagers, kitten thoughts): NekoLM writes them in the background while
 * play goes on, and each stays on screen for a few seconds. Lines wait their turn for the model; when too
 * many pile up, the least important waiting line is dropped. Blocking dialogue always goes first. */
#pragma once
#include "engine.h"

void chat_clear(void);
/* prio: 1 flavour, 2 matters (an order), 3 must show (someone storms out) */
void chat_say(const char *name, u16 color, int voice, int spk, int sit, const char *key, const char *value,
              int prio);
void chat_update(void);
void chat_draw(int x, int y, int w);    /* newest line: name tag + up to two rows of text */
int chat_idle(void);                    /* nothing waiting or being written */
int chat_showing(void);                 /* a line is on screen */
