/* Fallback teacher lines per (speaker, situation), baked from the distillation set. */
#pragma once
#include "engine.h"

typedef struct { u8 spk, sit; const char *const *lines; int count; } LinePool;
extern const LinePool line_pools[];
extern const int line_pool_count;

const char *line_pick(int spk, int sit);   /* random teacher line, or 0 if the pair has none */
