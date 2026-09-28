/* NekoLM integer inference: mirrors IntLM in tools/nekolm.py exactly (same shifts, clips and truncating
 * divisions), so the stick, the browser and the PC reference produce identical logits. */
#include "nekolm.h"

#define D NLM_D
#define T NLM_T
#define HD (NLM_D / NLM_H)

static i16 kc[NLM_L][T][D], vc[NLM_L][T][D];
static i32 x[D], logits[NLM_V];
static int npos;

static const i8 *const W_qkv[NLM_L] = { nlm_l0_qkv, nlm_l1_qkv, nlm_l2_qkv, nlm_l3_qkv };
static const i32 *const M_qkv[NLM_L] = { nlm_l0_qkv_m, nlm_l1_qkv_m, nlm_l2_qkv_m, nlm_l3_qkv_m };
static const i8 *const W_proj[NLM_L] = { nlm_l0_proj, nlm_l1_proj, nlm_l2_proj, nlm_l3_proj };
static const i32 *const M_proj[NLM_L] = { nlm_l0_proj_m, nlm_l1_proj_m, nlm_l2_proj_m, nlm_l3_proj_m };
static const i8 *const W_fc[NLM_L] = { nlm_l0_fc, nlm_l1_fc, nlm_l2_fc, nlm_l3_fc };
static const i32 *const M_fc[NLM_L] = { nlm_l0_fc_m, nlm_l1_fc_m, nlm_l2_fc_m, nlm_l3_fc_m };
static const i8 *const W_out[NLM_L] = { nlm_l0_out, nlm_l1_out, nlm_l2_out, nlm_l3_out };
static const i32 *const M_out[NLM_L] = { nlm_l0_out_m, nlm_l1_out_m, nlm_l2_out_m, nlm_l3_out_m };
static const i16 *const G1[NLM_L] = { nlm_l0_g1, nlm_l1_g1, nlm_l2_g1, nlm_l3_g1 };
static const i16 *const G2[NLM_L] = { nlm_l0_g2, nlm_l1_g2, nlm_l2_g2, nlm_l3_g2 };

static inline i32 clip(i32 v, i32 lim) { return v > lim ? lim : v < -lim ? -lim : v; }

static u64 isqrt64(u64 v)
{
    u64 r = 0, bit = (u64)1 << 62;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; }
        else r >>= 1;
        bit >>= 2;
    }
    return r;
}

static void rmsnorm_q8(const i32 *in, const i16 *g, i16 *out)
{
    i64 ss = 0;
    for (int i = 0; i < D; i++)
        ss += (i64)in[i] * in[i];
    i64 r = (i64)isqrt64((u64)(ss / D + 16));
    if (r < 1) r = 1;
    i64 den = r * 16;
    for (int i = 0; i < D; i++) {
        i64 v = ((i64)in[i] * g[i]) / den;            /* C truncates toward zero, like tdiv */
        out[i] = (i16)(v > 32767 ? 32767 : v < -32767 ? -32767 : v);
    }
}

/* ---------------------------------------------------------------- dot products
 * Every weight row times the activations is one of these (n is 128 or 512, a multiple of 4). All three return
 * the same exact integer: a row's sum fits in 32 bits (127 x 32767 x 512 < 2^31), so neither the order of the
 * additions nor a 64-bit accumulator changes it. Which is fastest depends on the CPU's multiplier (on the M15's
 * MIPS 74Kc a plain loop waits on every multiply), so nlm_pick_kernel times them on the device and keeps one. */
typedef i64 (*dot_fn)(const i8 *w, const i16 *a, int n);   /* 64-bit so madd keeps its accumulator */

static i64 dot_plain(const i8 *w, const i16 *a, int n)
{
    i32 acc = 0;
    for (int c = 0; c < n; c++)
        acc += w[c] * a[c];
    return acc;
}

#if defined(__mips__) && !defined(__mips64)
/* madd accumulates into the multiplier's own 64-bit HI/LO register, so back-to-back multiplies need no moves;
 * GCC will not emit it for this loop by itself ("x" = the HI/LO pair). */
#define MADD(acc, x, y) __asm__("madd %1, %2" : "+x"(acc) : "d"((i32)(x)), "d"((i32)(y)))
#else
#define MADD(acc, x, y) ((acc) += (i64)(i32)(x) * (y))
#endif

static i64 dot_madd(const i8 *w, const i16 *a, int n)      /* one 64-bit accumulator: MIPS madd */
{
    i64 acc = 0;
    for (int c = 0; c < n; c += 4) {
        MADD(acc, w[c], a[c]);
        MADD(acc, w[c + 1], a[c + 1]);
        MADD(acc, w[c + 2], a[c + 2]);
        MADD(acc, w[c + 3], a[c + 3]);
    }
    return acc;
}

static i64 dot_split(const i8 *w, const i16 *a, int n)     /* four independent sums: no multiply waits on the last */
{
    i32 s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    for (int c = 0; c < n; c += 4) {
        s0 += w[c] * a[c];
        s1 += w[c + 1] * a[c + 1];
        s2 += w[c + 2] * a[c + 2];
        s3 += w[c + 3] * a[c + 3];
    }
    return s0 + s1 + s2 + s3;
}

static dot_fn dot = dot_plain;
static const dot_fn kernels[3] = { dot_plain, dot_madd, dot_split };
static const char *const kernel_names[3] = { "plain", "madd", "split" };

const char *nlm_set_kernel(int k)
{
    if (k < 0 || k > 2)
        return 0;
    dot = kernels[k];
    return kernel_names[k];
}

static void matvec(const i8 *w, const i32 *m, const i16 *a, int rows, int cols, i32 *out)
{
    for (int r = 0; r < rows; r++)
        out[r] = (i32)((dot(w + r * cols, a, cols) * m[r]) >> 16);
}

void nlm_reset(void) { npos = 0; }
int nlm_cursor(void) { return npos; }
void nlm_rewind(int pos) { if (pos <= npos) npos = pos; }

/* ---------------------------------------------------------------- the forward pass, in slices
 * One token is nine slices: each layer's attention, each layer's feed-forward, then the output logits. A slice
 * is at most a fifth of a token's work, so callers can stop between slices and keep the frame rate on a slow
 * chip (on the M15 a whole token did not fit in a frame next to the game). */
static i16 a[4 * D];
static i32 qkv[3 * D], tmp[4 * D];
static i16 q[D], o[D];
static i32 w[T];
static int st_slice = -1, st_pos;

static void attention(int l, int pos)
{
    rmsnorm_q8(x, G1[l], a);
    matvec(W_qkv[l], M_qkv[l], a, 3 * D, D, qkv);
    for (int j = 0; j < D; j++) {
        q[j] = (i16)clip(qkv[j] >> 5, 8191);
        kc[l][pos][j] = (i16)clip(qkv[D + j] >> 5, 8191);
        vc[l][pos][j] = (i16)clip(qkv[2 * D + j] >> 4, 8191);
    }
    for (int h = 0; h < NLM_H; h++) {
        int j0 = h * HD;
        i32 mx = -0x7FFFFFFF;
        for (int t = 0; t <= pos; t++) {
            i32 acc = 0;
            for (int j = 0; j < HD; j++)
                acc += kc[l][t][j0 + j] * q[j0 + j];
            w[t] = acc >> 6;
            if (w[t] > mx) mx = w[t];
        }
        i32 sum = 0;
        for (int t = 0; t <= pos; t++) {
            i32 d = mx - w[t];
            w[t] = nlm_exp[d > 4095 ? 4095 : d];
            sum += w[t];
        }
        for (int t = 0; t <= pos; t++)
            w[t] = (w[t] << 12) / sum;
        for (int j = 0; j < HD; j++) {
            i32 acc = 0;
            for (int t = 0; t <= pos; t++)
                acc += w[t] * vc[l][t][j0 + j];
            o[j0 + j] = (i16)clip(acc >> 12, 32767);
        }
    }
    matvec(W_proj[l], M_proj[l], o, D, D, tmp);
    for (int i = 0; i < D; i++)
        x[i] += tmp[i];
}

static void feed_forward(int l)
{
    rmsnorm_q8(x, G2[l], a);
    matvec(W_fc[l], M_fc[l], a, 4 * D, D, tmp);
    for (int i = 0; i < 4 * D; i++)
        a[i] = (i16)clip((tmp[i] < 0 ? 0 : tmp[i]) >> 4, 8191);
    matvec(W_out[l], M_out[l], a, D, 4 * D, tmp);
    for (int i = 0; i < D; i++)
        x[i] += tmp[i];
}

static void output(void)
{
    rmsnorm_q8(x, nlm_gf, a);
    for (int t = 0; t < NLM_V; t++)
        logits[t] = (i32)((dot(nlm_tok + t * D, a, D) * nlm_tok_m[t]) >> 20);
}

void nlm_feed(int tok)
{
    st_pos = npos;
    if (st_pos >= T) {                               /* context full: the logits stay as they were */
        st_slice = -1;
        return;
    }
    for (int i = 0; i < D; i++)
        x[i] = ((nlm_tok[tok * D + i] * nlm_tok_m[tok]) >> 8) + nlm_pos[st_pos * D + i];
    st_slice = 0;
}

int nlm_slice(void)
{
    if (st_slice < 0)
        return 1;
    if (st_slice < 2 * NLM_L) {
        if (st_slice & 1)
            feed_forward(st_slice >> 1);
        else
            attention(st_slice >> 1, st_pos);
        st_slice++;
        return 0;
    }
    npos++;
    output();
    st_slice = -1;
    return 1;
}

int nlm_busy(void) { return st_slice >= 0; }

static char *put_s(char *p, const char *s)
{
    while (*s) *p++ = *s++;
    *p = 0;
    return p;
}

static char *put_u(char *p, u32 v)
{
    char tmp[12];
    int n = 0;
    do tmp[n++] = (char)('0' + v % 10); while ((v /= 10));
    while (n) *p++ = tmp[--n];
    *p = 0;
    return p;
}

/* Time each dot product on the first feed-forward matrix (512 rows of 128) and keep the fastest one that gives
 * exactly the plain loop's sums, including on the largest sums the weights allow. The kernels take turns over
 * five rounds and each keeps its best round, so an interruption during one round cannot decide the choice.
 * Writes a line for the log (microseconds for 2 x 512 rows). */
void nlm_pick_kernel(char *report)
{
    const i8 *w = W_fc[0];
    static i16 act[2][D];
    for (int j = 0; j < D; j++) {
        act[0][j] = (i16)((int)((u32)(j + 1) * 2654435761u >> 18) % 16383 - 8191);   /* spread-out values */
        act[1][j] = (i16)(w[j] < 0 ? -32767 : 32767);                             /* the largest sum row 0 allows */
    }
    int ok[3], best = 0;
    u32 us[3];
    for (int k = 0; k < 3; k++) {
        ok[k] = 1;
        us[k] = 0xFFFFFFFFu;
        for (int v = 0; v < 2; v++)
            for (int r = 0; r < 4 * D; r++)
                if (kernels[k](w + r * D, act[v], D) != dot_plain(w + r * D, act[v], D))
                    ok[k] = 0;
    }
    volatile i64 sink = 0;
    for (int round = 0; round < 5; round++)
        for (int k = 0; k < 3; k++) {
            u32 t0 = time_us();
            for (int rep = 0; rep < 2; rep++)
                for (int r = 0; r < 4 * D; r++)
                    sink += kernels[k](w + r * D, act[0], D);
            u32 d = time_us() - t0;
            if (d < us[k])
                us[k] = d;
        }
    char *p = put_s(report, "kernel");
    for (int k = 0; k < 3; k++) {
        p = put_u(put_s(put_s(put_s(p, " "), kernel_names[k]), "="), us[k]);
        p = put_s(p, ok[k] ? "us" : "us(WRONG)");
        if (ok[k] && us[k] < us[best])
            best = k;
    }
    put_s(put_s(p, " -> "), kernel_names[best]);
    dot = kernels[best];
}

const i32 *nlm_forward(int tok)
{
    nlm_feed(tok);
    while (!nlm_slice()) {}
    return logits;
}

/* log p(tok) under the current logits, in 1/256 nats (never positive): the composer ranks candidate lines by it. */
i32 nlm_logprob_q8(int tok)
{
    i32 mx = logits[0];
    for (int t = 1; t < NLM_V; t++)
        if (logits[t] > mx) mx = logits[t];
    u32 s = 0;
    for (int t = 0; t < NLM_V; t++) {
        i32 d = mx - logits[t];
        s += nlm_exp[d > 4095 ? 4095 : d];
    }
    int b = 0;                                       /* log2(s) in Q8: the top bit, plus a curve for the rest */
    while ((s >> b) > 1) b++;
    u32 m = b >= 16 ? s >> (b - 16) : s << (16 - b);  /* 1.16 fixed point, 65536..131071 */
    u32 f = m - 65536;
    i32 log2_q8 = b * 256 + (i32)(((u64)f * (88244u - ((f * 22708u) >> 16))) >> 24);
    i32 ln_q8 = (log2_q8 - 15 * 256) * 709 / 1024;   /* ln(s / 32768) = (log2 s - 15) ln 2 */
    return logits[tok] - mx - ln_q8;
}

/* ---------------------------------------------------------------- sampling */

#define TOPK_MAX 16
int nlm_topk = 6;                /* sample among the k likeliest tokens */
int nlm_temp_q8 = 384;           /* 256 / temperature: 384 = 0.67 (89% of lines pass nlm_ok vs 70% at 1.0) */

static char gen_line[NLM_MAXLEN + 1];
static int line_len, gen_state;   /* 0 idle, 1 prefix fed next, 2 generating, 3 done */
static int ended_nl;              /* the model ended the line itself (not cut at a length limit) */
static int prefix[3], prefix_i;
static u32 gen_rng, ticket;

static u32 gen_rand(void)
{
    gen_rng ^= gen_rng << 13;
    gen_rng ^= gen_rng >> 17;
    gen_rng ^= gen_rng << 5;
    return gen_rng;
}

static int pick_token(void)
{
    int best_t[TOPK_MAX];
    i32 best_z[TOPK_MAX];
    int n = 0;
    for (int tok = 10; tok < NLM_V; tok++) {   /* end of line, printable bytes and the subword merges */
        if (tok == 11) tok = 32;
        if (tok == 127) tok = 256;
        if (tok >= NLM_V) break;
        i32 z = (logits[tok] * nlm_temp_q8) >> 8;
        if (n < nlm_topk) {
            best_t[n] = tok; best_z[n] = z; n++;
        } else {
            int lo = 0;
            for (int k = 1; k < nlm_topk; k++) if (best_z[k] < best_z[lo]) lo = k;
            if (z > best_z[lo]) { best_t[lo] = tok; best_z[lo] = z; }
        }
    }
    i32 mx = best_z[0];
    for (int k = 1; k < n; k++) if (best_z[k] > mx) mx = best_z[k];
    u32 wsum = 0, wt[TOPK_MAX];
    for (int k = 0; k < n; k++) {
        i32 d = mx - best_z[k];
        wt[k] = nlm_exp[d > 4095 ? 4095 : d];
        wsum += wt[k];
    }
    u32 r = gen_rand() % (wsum ? wsum : 1);
    for (int k = 0; k < n; k++) {
        if (r < wt[k]) return best_t[k];
        r -= wt[k];
    }
    return best_t[0];
}

u32 nlm_claim(void)
{
    npos = 0;
    st_slice = -1;
    gen_state = 0;
    return ++ticket;
}

u32 nlm_begin(int spk, int sit, u32 seed)
{
    u32 t = nlm_claim();
    prefix[0] = 10; prefix[1] = spk; prefix[2] = sit;
    prefix_i = 0;
    line_len = 0;
    gen_line[0] = 0;
    ended_nl = 0;
    gen_rng = seed ? seed : 0x9E3779B9u;
    gen_state = 1;
    return t;
}

/* One slice of line-writing work; 1 once the line is finished. */
static int gen_work(void)
{
    if (gen_state == 3 || gen_state == 0)
        return 1;
    if (st_slice >= 0) {
        nlm_slice();
        return 0;
    }
    if (gen_state == 1) {
        nlm_feed(prefix[prefix_i++]);
        if (prefix_i == 3) gen_state = 2;
        return 0;
    }
    int t = pick_token();
    int off = nlm_str_off[t], len = nlm_str_off[t + 1] - off;
    if (t == 10 || line_len + len > NLM_MAXLEN || npos >= T) {
        ended_nl = t == 10;
        gen_state = 3;
        return 1;
    }
    for (int i = 0; i < len; i++)
        gen_line[line_len++] = (char)nlm_str[off + i];
    gen_line[line_len] = 0;
    nlm_feed(t);
    return 0;
}

int nlm_run(int max_tokens)
{
    while (max_tokens-- > 0 && gen_state != 3) {
        gen_work();                                  /* feed the next token (or finish the line) */
        while (st_slice >= 0)                        /* and run its whole forward pass */
            nlm_slice();
    }
    return gen_state == 3;
}

int nlm_done(void) { return gen_state == 3; }
int nlm_complete(void) { return gen_state == 3 && ended_nl; }
const char *nlm_text(void) { return gen_line; }

const char *nlm_line(int spk, int sit, u32 seed)
{
    nlm_begin(spk, sit, seed);
    while (!nlm_run(64)) {}
    return gen_line;
}


/* Does the value start with a vowel sound ("espresso", "Egg Carton", 8, 11, 18)? For the article before it. */
static int vowel_sound(const char *v)
{
    char c = (char)(v[0] | 32);
    if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u')
        return 1;
    return v[0] == '8' || (v[0] == '1' && (v[1] == '1' || v[1] == '8') && !(v[2] >= '0' && v[2] <= '9'));
}

static int alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

void nlm_fill(char *dst, const char *src, const char *key, const char *value)
{
    int klen = str_len(key), n = 0;
    for (int i = 0; src[i] && n < 126; ) {
        if (src[i] == '{') {
            int j = 0;
            while (j < klen && src[i + 1 + j] == key[j]) j++;
            if (j == klen && src[i + 1 + j] == '}') {
                /* "a espresso" -> "an espresso", "an fish" -> "a fish" (tools/nlm_quality.py c_fill mirrors this) */
                if (n >= 2 && n < 125 && dst[n - 1] == ' ' && (dst[n - 2] | 32) == 'a' && (n == 2 || !alpha(dst[n - 3]))
                    && vowel_sound(value)) {
                    dst[n - 1] = 'n';
                    dst[n++] = ' ';
                } else if (n >= 3 && dst[n - 1] == ' ' && dst[n - 2] == 'n' && (dst[n - 3] | 32) == 'a'
                           && (n == 3 || !alpha(dst[n - 4])) && !vowel_sound(value)) {
                    dst[n - 2] = ' ';
                    n--;
                }
                int start = n == 0 || (n >= 2 && dst[n - 1] == ' ' && (dst[n - 2] == '!' || dst[n - 2] == '?'
                                        || (dst[n - 2] == '.' && (n < 3 || dst[n - 3] != '.'))));
                for (int v = 0; value[v] && n < 126; v++) {
                    char c = value[v];
                    if (v == 0 && start && c >= 'a' && c <= 'z') c = (char)(c - 32);   /* starts a sentence */
                    dst[n++] = c;
                }
                i += klen + 2;
                continue;
            }
        }
        dst[n++] = src[i++];
    }
    dst[n] = 0;
}

/* Is this word (letters and apostrophes, any case) one the teacher used? FNV-1a of the lowercase word,
 * looked up in the sorted hash list exported with the model. */
static int known_word(const char *w, int n)
{
    while (n && w[0] == '\'') { w++; n--; }
    while (n && w[n - 1] == '\'') n--;
    if (!n)
        return 1;
    u32 h = 2166136261u;
    for (int i = 0; i < n; i++) {
        char c = w[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        h = (h ^ (u8)c) * 16777619u;
    }
    int lo = 0, hi = NLM_WORDS - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (nlm_words[mid] == h) return 1;
        if (nlm_words[mid] < h) lo = mid + 1; else hi = mid - 1;
    }
    return 0;
}

static int is_letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '\''; }

static int same_word(const char *s, int a, int al, int b, int bl)
{
    if (al != bl)
        return 0;
    for (int k = 0; k < al; k++)
        if (s[a + k] != s[b + k]) return 0;
    return 1;
}

/* A line is shown only if it is long enough, uses no placeholder except {key} (and does use it when a key
 * is given), spells only words the teacher used, and has no stutter: a character four times running, or a
 * word or two-word phrase repeated back to back. */
int nlm_ok(const char *s, const char *key)
{
    int klen = key ? str_len(key) : 0, uses = 0;
    if (str_len(s) < 8)
        return 0;
    for (int i = 0; s[i]; ) {
        if (s[i] == '{') {                         /* placeholders are checked below */
            while (s[i] && s[i] != '}') i++;
            continue;
        }
        if (!is_letter(s[i])) { i++; continue; }
        int j = i;
        while (is_letter(s[j])) j++;
        if (!known_word(s + i, j - i))
            return 0;
        i = j;
    }
    for (int i = 0; s[i]; i++) {
        if (s[i] == '}')
            return 0;
        if (s[i] == '{') {
            int j = 0;
            while (j < klen && s[i + 1 + j] == key[j]) j++;
            if (!klen || j < klen || s[i + 1 + klen] != '}')
                return 0;
            uses++;
            i += klen + 1;
            continue;
        }
        if (i >= 3 && s[i] == s[i - 1] && s[i] == s[i - 2] && s[i] == s[i - 3])
            return 0;
    }
    if (klen && !uses)
        return 0;
    /* split into words, then reject a word or a two-word phrase repeated back to back ("Paw hockey. Paw hockey.") */
    int ws[48], wl[48], nw = 0;
    for (int i = 0; s[i] && nw < 48; ) {
        while (s[i] == ' ') i++;
        if (!s[i]) break;
        ws[nw] = i;
        while (s[i] && s[i] != ' ') i++;
        wl[nw] = i - ws[nw];
        nw++;
    }
    for (int i = 0; i + 1 < nw; i++) {
        if (wl[i] >= 2 && same_word(s, ws[i], wl[i], ws[i + 1], wl[i + 1]))
            return 0;
        if (i + 3 < nw && same_word(s, ws[i], wl[i], ws[i + 2], wl[i + 2])
            && same_word(s, ws[i + 1], wl[i + 1], ws[i + 3], wl[i + 3]))
            return 0;
    }
    return 1;
}

/* ---------------------------------------------------------------- time slicing
 * All model work in a frame shares one budget, nlm_frame_budget_us, which the game loop sets each frame from
 * the time its own work leaves; nlm_frame_us is how much of it the model has used so far this frame. */
int nlm_frame_budget_us = 3000;
u32 nlm_frame_us, nlm_stat_slices, nlm_stat_us, nlm_stat_max_us;
static u32 slice_avg = 1000;                         /* running average of a slice's cost on this machine */

int nlm_work_for(int (*work)(void *), void *ctx, int max_slices)
{
    u32 t0 = time_us();
    if (!t0)
        max_slices *= 9;                             /* no clock: a fixed amount of work */
    else if (nlm_frame_us && nlm_frame_us + slice_avg >= (u32)nlm_frame_budget_us)
        return 0;                                    /* no room left this frame (the first call always gets one) */
    int done = 0, i = 0;
    while (i < max_slices && !done) {
        u32 s0 = t0 ? time_us() : 0;
        done = work(ctx);
        i++;
        if (t0) {
            u32 now = time_us(), d = now - s0;
            if (d > nlm_stat_max_us)
                nlm_stat_max_us = d;
            slice_avg = (slice_avg * 7 + d) / 8;
            /* only start another slice if it should still fit in this frame's budget */
            if (nlm_frame_us + (now - t0) + slice_avg >= (u32)nlm_frame_budget_us)
                break;
        }
    }
    nlm_stat_slices += (u32)i;
    if (t0) {
        u32 used = time_us() - t0;
        nlm_stat_us += used;
        nlm_frame_us += used;
    }
    return done;
}

static int gen_work_cb(void *ctx)
{
    (void)ctx;
    return gen_work();
}

int nlm_run_for(int max_us, int max_slices)
{
    (void)max_us;
    if (gen_state == 3)
        return 1;
    return nlm_work_for(gen_work_cb, 0, max_slices);
}

u32 nlm_current(void) { return ticket; }
