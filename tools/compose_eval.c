/* The game's own composer (src/live.c) run for every speaker/situation, for the quality panel:
 *   compose_eval N > build/quality/composer.tsv      spk <tab> sit <tab> line (placeholders left in)
 * Built by tools/composeeval.sh. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/live.h"
#include "../src/nekolm.h"

int str_len(const char *s) { return (int)strlen(s); }
char *str_cpy(char *d, const char *s) { while ((*d = *s++)) d++; return d; }
u32 time_us(void) { return 0; }
static u32 rs = 4242;
u32 rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

int main(int argc, char **argv)
{
    int n = argc > 1 ? atoi(argv[1]) : 2;
    for (int p = 0; p < cpool_count; p++) {
        const CPool *cp = &cpools[p];
        static const char *const keys[4] = { 0, "item", "n", "dir" }, *const self[4] = { 0, "{item}", "{n}", "{dir}" };
        const char *key = keys[cp->key];
        for (int k = 0; k < n; k++) {
            LiveJob j;
            live_init(&j, cp->spk, cp->sit, key, self[cp->key]);     /* placeholders stay for the panel to fill */
            while (!live_step(&j, 0, 64)) {}
            printf("%d\t%d\t%s\n", cp->spk, cp->sit, j.out);
        }
    }
    fprintf(stderr, "composed %u, whole teacher lines %u\n", live_stat_composed, live_stat_whole);
    return 0;
}
