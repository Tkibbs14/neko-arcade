/* The game's composer scoring (src/live.c, integer NekoLM) on given candidate sets, for the lab:
 *   rank_eval < groups.tsv > scores.tsv
 * Input lines: speaker tag, situation tag, opener index, six rest indices (indices into the pool, as in
 * build/compose_bank.json). Output: the six scores the device would compare. Built by tools/rankeval.sh with
 * LIVE_CANDS=6; the device itself compares the first three. */
#include <stdio.h>
#include <string.h>
#include "../src/live.h"
#include "../src/nekolm.h"

int str_len(const char *s) { return (int)strlen(s); }
char *str_cpy(char *d, const char *s) { while ((*d = *s++)) d++; return d; }
u32 time_us(void) { return 0; }
u32 rnd(void) { return 0; }

int main(void)
{
    int spk, sit, op, r[6];
    while (scanf("%d %d %d %d %d %d %d %d %d", &spk, &sit, &op, &r[0], &r[1], &r[2], &r[3], &r[4], &r[5]) == 9) {
        const CPool *p = 0;
        for (int i = 0; i < cpool_count; i++)
            if (cpools[i].spk == spk && cpools[i].sit == sit) p = &cpools[i];
        if (!p || op >= p->nop) { printf("x\n"); continue; }
        LiveJob j;
        live_init(&j, spk, sit, 0, "");
        j.pool = p;
        j.op = (i16)op;
        for (int k = 0; k < 6; k++) j.cand[k] = (i16)r[k];
        j.ncand = 6;
        while (!live_step(&j, 0, 64)) {}
        for (int k = 0; k < 6; k++) printf(k ? "\t%d" : "%d", (int)j.score[k]);
        printf("\n");
    }
    return 0;
}
