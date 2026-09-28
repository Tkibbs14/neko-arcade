/* The composer's sentence bank: the teacher's lines split into an opening sentence and the rest, tokenized so
 * NekoLM can score on the device how naturally a rest follows an opener. Baked by tools/mkcompose.py. */
#pragma once
#include "engine.h"

typedef struct {
    const char *text;
    const u16 *tok;
    u8 ntok, src, key, len;          /* src: which teacher line it came from; key: holds the placeholder */
} CUnit;

typedef struct {
    u8 spk, sit, key;                /* key: the situation's placeholder, 0 none, 1 {item}, 2 {n}, 3 {dir} */
    const CUnit *op, *rest;
    u8 nop, nrest;
    const u16 *ok;                   /* approved pairs, opener << 8 | rest, sorted: three large models rated each */
    u16 nok;                         /* combination fully sensible (tools/mkcompose.py); 0 = none rated yet */
} CPool;

extern const CPool cpools[];
extern const int cpool_count;
