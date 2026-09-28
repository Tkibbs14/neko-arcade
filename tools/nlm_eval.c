/* NekoLM evaluation set: lines from the exact on-device sampler for every speaker/situation pair the game
 * uses, each marked with whether the on-device check (nlm_ok) passes it.
 *   nlm_eval PASS_TARGET MAX_TRIES [TEMP_Q8 TOPK]  > build/quality/student.tsv
 * Per pair it samples until PASS_TARGET lines pass or MAX_TRIES are used; every attempt is written:
 *   spk <tab> sit <tab> attempt <tab> pass <tab> line
 * Built by tools/quality.sh. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/nekolm.h"
#include "../src/lines.h"

int str_len(const char *s) { return (int)strlen(s); }
u32 time_us(void) { return 0; }
static u32 rs = 777;
u32 rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

static const char *key_of(const LinePool *p)
{
    static const char *const keys[] = { "item", "n", "dir" };
    for (int k = 0; k < 3; k++) {
        char pat[8];
        snprintf(pat, sizeof pat, "{%s}", keys[k]);
        for (int i = 0; i < p->count; i++)
            if (strstr(p->lines[i], pat)) return keys[k];
    }
    return 0;
}

int main(int argc, char **argv)
{
    int target = argc > 1 ? atoi(argv[1]) : 3, tries = argc > 2 ? atoi(argv[2]) : 12;
    if (argc > 4) { nlm_temp_q8 = atoi(argv[3]); nlm_topk = atoi(argv[4]); }
    for (int p = 0; p < line_pool_count; p++) {
        const LinePool *lp = &line_pools[p];
        int passed = 0;
        for (int a = 0; a < tries && passed < target; a++) {
            nlm_begin(lp->spk, lp->sit, 2654435761u * (u32)(p + 1) + 40503u * (u32)(a + 1));
            while (!nlm_run(8)) {}
            int ok = nlm_complete() && nlm_ok(nlm_text(), key_of(lp));
            passed += ok;
            printf("%d\t%d\t%d\t%d\t%s\n", lp->spk, lp->sit, a, ok, nlm_text());
        }
    }
    return 0;
}
