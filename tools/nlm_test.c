/* NekoLM parity and speed check: the C engine must reproduce the integer reference in tools/nekolm.py
 * exactly (build/nekolm_vectors.txt), then time line generation on this host.
 *   nlm_test VECTORS_FILE [lines]      parity, sample lines, speed
 *   nlm_test VECTORS_FILE N sweep      also: share of lines passing the on-device checks, per sampling setting,
 *                                      over every speaker/situation pair (N lines each)
 * Built natively by tools/nlmtest.sh. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../src/nekolm.h"
#include "../src/lines.h"

int str_len(const char *s) { return (int)strlen(s); }
u32 time_us(void) { return 0; }
static u32 rs = 12345;
u32 rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

static double now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static const char *key_of(const LinePool *p)
{
    static const char *const keys[] = { "item", "n", "dir" };
    for (int k = 0; k < 3; k++) {
        char pat[8];
        snprintf(pat, sizeof pat, "{%s}", keys[k]);
        if (strstr(p->lines[0], pat)) return keys[k];
    }
    return 0;
}

static int parity(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) { perror("vectors"); return -1; }
    char buf[512];
    int step = 0, bad = 0;
    nlm_reset();
    while (fgets(buf, sizeof buf, f)) {
        char *p = buf;
        int tok = (int)strtol(p, &p, 10);
        const i32 *lg = nlm_forward(tok);
        for (;;) {
            while (*p == ' ') p++;
            if (!strncmp(p, "sum:", 4)) {
                long long want = strtoll(p + 4, 0, 10), got = 0;
                for (int i = 0; i < NLM_V; i++) got += lg[i];
                if (got != want) { printf("step %d: logit sum %lld, want %lld\n", step, got, want); bad++; }
                break;
            }
            int idx = (int)strtol(p, &p, 10);
            if (*p != ':') break;
            long want = strtol(p + 1, &p, 10);
            if (lg[idx] != want) { printf("step %d: logit[%d] = %d, want %ld\n", step, idx, lg[idx], want); bad++; }
        }
        step++;
    }
    fclose(f);
    printf("parity: %d steps, %s\n", step, bad ? "MISMATCH" : "exact match");
    return bad;
}

int main(int argc, char **argv)
{
    for (int k = 1; nlm_set_kernel(k); k++) {        /* every dot-product kernel must match exactly too */
        printf("kernel %s: ", nlm_set_kernel(k));
        if (parity(argc > 1 ? argv[1] : "build/nekolm_vectors.txt"))
            return 1;
    }
    nlm_set_kernel(0);
    printf("kernel plain: ");
    if (parity(argc > 1 ? argv[1] : "build/nekolm_vectors.txt"))
        return 1;
    int lines = argc > 2 ? atoi(argv[2]) : 40, tokens = 0, ok = 0;
    static const struct { int spk, sit; const char *name; } probe[] = {
        { SPK_SHIO, SIT_D_EVIDENCE, "Shio/evidence" }, { SPK_MAKO, SIT_R_YOU_SCORE, "Mako/you score" },
        { SPK_NIA, SIT_HUB_HELLO, "Nia/hello" }, { SPK_MOCHA, SIT_C_RUSH, "Mocha/rush" },
        { SPK_KITTEN, SIT_K_HUNGRY, "Kitten/hungry" }, { SPK_KURO, SIT_V_SMALLTALK, "Kuro/smalltalk" },
    };
    double t0 = now();
    for (int i = 0; i < lines; i++) {
        int k = i % 6;
        nlm_begin(probe[k].spk, probe[k].sit, 1234u + (u32)i * 7919u);
        while (!nlm_run(1)) tokens++;
        int good = nlm_complete() && nlm_ok(nlm_text(), probe[k].sit == SIT_D_EVIDENCE ? "item" : 0);
        ok += good;
        if (i < 18)
            printf("  %-15s %s %s\n", probe[k].name, good ? " " : "x", nlm_text());
    }
    double dt = now() - t0;
    printf("speed: %d lines, %d tokens in %.3f s = %.2f ms/token, %.1f ms/line; %d/%d pass the checks\n",
           lines, tokens, dt, dt * 1000 / tokens, dt * 1000 / lines, ok, lines);
    if (argc < 4 || strcmp(argv[3], "sweep"))
        return 0;
    static const int temps[] = { 256, 320, 384 }, topks[] = { 6, 8, 12 };
    for (int a = 0; a < 3; a++)
        for (int b = 0; b < 3; b++) {
            nlm_temp_q8 = temps[a];
            nlm_topk = topks[b];
            int n = 0, pass = 0, len = 0;
            for (int p = 0; p < line_pool_count; p++)
                for (int r = 0; r < lines; r++) {
                    nlm_begin(line_pools[p].spk, line_pools[p].sit, 99u + (u32)(p * 131 + r * 7));
                    while (!nlm_run(8)) {}
                    int good = nlm_complete() && nlm_ok(nlm_text(), key_of(&line_pools[p]));
                    pass += good;
                    len += good ? str_len(nlm_text()) : 0;
                    n++;
                }
            printf("sweep: temperature %.2f top-%-2d  %4d/%d pass (%.0f%%), mean passing length %.0f chars\n",
                   256.0 / temps[a], topks[b], pass, n, 100.0 * pass / n, pass ? (double)len / pass : 0.0);
        }
    nlm_temp_q8 = 384;
    nlm_topk = 6;
    return 0;
}
