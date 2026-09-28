/* Neko Arcade engine core: frame buffer, input, drawing, text, integer math, strings. */
#include "engine.h"

u16 fb[SCREEN_W * SCREEN_H] __attribute__((aligned(16)));   /* cls() writes it as 32-bit words */
u32 frame_count;

/* ---------------------------------------------------------------- input */

static u16 pad_now[2], pad_prev[2];
static u16 pad_hold_frames[2][16];

void input_update(u16 p1, u16 p2)
{
    pad_prev[0] = pad_now[0];
    pad_prev[1] = pad_now[1];
    pad_now[0] = p1;
    pad_now[1] = p2;
    for (int p = 0; p < 2; p++)
        for (int b = 0; b < 16; b++)
            pad_hold_frames[p][b] = (pad_now[p] >> b & 1) ? (u16)(pad_hold_frames[p][b] + 1) : 0;
    if ((p1 | p2) & ~(pad_prev[0] | pad_prev[1]))
        rnd_mix(frame_count * 2654435761u ^ p1 ^ (u32)p2 << 16);
}

int btn_held(int pl, u16 m) { return (pad_now[pl] & m) != 0; }
int btn_pressed(int pl, u16 m) { return (pad_now[pl] & ~pad_prev[pl] & m) != 0; }
int btn_released(int pl, u16 m) { return (~pad_now[pl] & pad_prev[pl] & m) != 0; }

int btn_repeat(int pl, u16 m)
{
    for (int b = 0; b < 16; b++) {
        if (!(m >> b & 1))
            continue;
        u16 f = pad_hold_frames[pl][b];
        if (f == 1 || (f >= 18 && (f - 18) % 5 == 0))
            return 1;
    }
    return 0;
}

int any_pressed(u16 m) { return btn_pressed(0, m) || btn_pressed(1, m); }
int any_repeat(u16 m) { return btn_repeat(0, m) || btn_repeat(1, m); }

/* ---------------------------------------------------------------- drawing */

void cls(u16 c)
{
    u32 cc = c | (u32)c << 16;
    u32 *p = (u32 *)fb;
    for (int i = 0; i < SCREEN_W * SCREEN_H / 2; i++)
        p[i] = cc;
}

void pset(int x, int y, u16 c)
{
    if ((unsigned)x < SCREEN_W && (unsigned)y < SCREEN_H)
        fb[y * SCREEN_W + x] = c;
}

void rect(int x, int y, int w, int h, u16 c)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_W) w = SCREEN_W - x;
    if (y + h > SCREEN_H) h = SCREEN_H - y;
    if (w <= 0 || h <= 0)
        return;
    for (int j = 0; j < h; j++) {
        u16 *p = fb + (y + j) * SCREEN_W + x;
        for (int i = 0; i < w; i++)
            p[i] = c;
    }
}

u16 blend(u16 a, u16 b, int t)
{
    if (t <= 0) return a;
    if (t >= 256) return b;
    int ar = a >> 11, ag = a >> 5 & 63, ab = a & 31;
    int br = b >> 11, bg = b >> 5 & 63, bb = b & 31;
    int r = ar + (((br - ar) * t) >> 8);
    int g = ag + (((bg - ag) * t) >> 8);
    int bl = ab + (((bb - ab) * t) >> 8);
    return (u16)(r << 11 | g << 5 | bl);
}

void rect_blend(int x, int y, int w, int h, u16 c, int a)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_W) w = SCREEN_W - x;
    if (y + h > SCREEN_H) h = SCREEN_H - y;
    if (w <= 0 || h <= 0)
        return;
    for (int j = 0; j < h; j++) {
        u16 *p = fb + (y + j) * SCREEN_W + x;
        for (int i = 0; i < w; i++)
            p[i] = blend(p[i], c, a);
    }
}

void dither_blend(int x, int y, int w, int h, u16 c, int level)
{
    static const u8 bayer[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
    int thr = level * 4;   /* level 0..4 -> 0..16 of 16 */
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            if (bayer[(y + j) & 3][(x + i) & 3] < thr)
                pset(x + i, y + j, c);
}

void hline(int x, int y, int w, u16 c) { rect(x, y, w, 1, c); }
void vline(int x, int y, int h, u16 c) { rect(x, y, 1, h, c); }

void rect_line(int x, int y, int w, int h, u16 c)
{
    hline(x, y, w, c);
    hline(x, y + h - 1, w, c);
    vline(x, y, h, c);
    vline(x + w - 1, y, h, c);
}

void round_box(int x, int y, int w, int h, u16 border, u16 fill)
{
    rect(x + 1, y + 1, w - 2, h - 2, fill);
    hline(x + 2, y, w - 4, border);
    hline(x + 2, y + h - 1, w - 4, border);
    vline(x, y + 2, h - 4, border);
    vline(x + w - 1, y + 2, h - 4, border);
    pset(x + 1, y + 1, border);
    pset(x + w - 2, y + 1, border);
    pset(x + 1, y + h - 2, border);
    pset(x + w - 2, y + h - 2, border);
}

void line(int x0, int y0, int x1, int y1, u16 c)
{
    int dx = iabs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -iabs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        pset(x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void ellipse_fill(int cx, int cy, int rx, int ry, u16 c)
{
    if (rx <= 0 || ry <= 0)
        return;
    for (int dy = -ry; dy <= ry; dy++) {
        /* half-width at this row: rx * sqrt(1 - (dy/ry)^2) */
        u32 t = (u32)(ry * ry - dy * dy) * 65536u / (u32)(ry * ry);
        int hw = (int)((isqrt(t) * (u32)rx + 128) >> 8);
        hline(cx - hw, cy + dy, 2 * hw + 1, c);
    }
}

void circle_fill(int cx, int cy, int r, u16 c) { ellipse_fill(cx, cy, r, r, c); }

void circle_line(int cx, int cy, int r, u16 c)
{
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        pset(cx + x, cy + y, c); pset(cx + y, cy + x, c);
        pset(cx - y, cy + x, c); pset(cx - x, cy + y, c);
        pset(cx - x, cy - y, c); pset(cx - y, cy - x, c);
        pset(cx + y, cy - x, c); pset(cx + x, cy - y, c);
        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
    }
}

void gradient_v(int x, int y, int w, int h, u16 top, u16 bottom)
{
    for (int j = 0; j < h; j++)
        hline(x, y + j, w, blend(top, bottom, h > 1 ? j * 256 / (h - 1) : 0));
}

/* ---------------------------------------------------------------- sprites */

int spr_w(int id) { return sprites[id].w; }
int spr_h(int id) { return sprites[id].h; }

static void spr_draw(int id, int x, int y, int flags, const u16 *ramp, int scale, u16 tint, int talpha)
{
    const Sprite *s = &sprites[id];
    const u16 *pal = s->ramp ? ramp : global_palette;
    if (!pal)
        return;
    for (int j = 0; j < s->h; j++) {
        int sy = y + j * scale;
        if (sy >= SCREEN_H || sy + scale <= 0)
            continue;
        const u8 *row = s->px + j * s->w;
        for (int i = 0; i < s->w; i++) {
            u8 v = row[(flags & FLIP_X) ? s->w - 1 - i : i];
            if (!v)
                continue;
            u16 c = pal[v];
            if (talpha)
                c = blend(c, tint, talpha);
            int sx = x + i * scale;
            if (scale == 1)
                pset(sx, sy, c);
            else
                rect(sx, sy, scale, scale, c);
        }
    }
}

void spr(int id, int x, int y, int flags) { spr_draw(id, x, y, flags, 0, 1, 0, 0); }
void spr_ramp(int id, int x, int y, int flags, const u16 *ramp) { spr_draw(id, x, y, flags, ramp, 1, 0, 0); }
void spr_scaled(int id, int x, int y, int scale, int flags, const u16 *ramp) { spr_draw(id, x, y, flags, ramp, scale, 0, 0); }
void spr_tinted(int id, int x, int y, int flags, const u16 *ramp, u16 tint, int alpha) { spr_draw(id, x, y, flags, ramp, 1, tint, alpha); }

/* ---------------------------------------------------------------- text */

static const u16 text_colors[10] = {
    0, C_PINK, C_GOLD, C_MINT, C_SKY, C_ORANGE, C_VIOLET, C_RED, C_GREY, C_WHITE,
};

static int glyph_w(unsigned char ch)
{
    if (ch >= 128)
        return 3;
    return font_main[ch].w;
}

static int draw_glyph(int x, int y, unsigned char ch, u16 c, int scale)
{
    if (ch >= 128)
        ch = '?';
    const Glyph *g = &font_main[ch];
    for (int r = 0; r < 8; r++) {
        u8 bits = g->rows[r];
        for (int b = 0; bits; b++, bits >>= 1)
            if (bits & 1) {
                if (scale == 1)
                    pset(x + b, y + r, c);
                else
                    rect(x + b * scale, y + r * scale, scale, scale, c);
            }
    }
    return g->w;
}

int text_n(int x, int y, const char *s, int n, u16 c)
{
    int x0 = x, maxx = x;
    u16 cur = c;
    for (int i = 0; s[i] && i < n; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == '^' && s[i + 1] >= '0' && s[i + 1] <= '9') {
            int k = s[i + 1] - '0';
            cur = k ? text_colors[k] : c;
            i++;
            continue;
        }
        if (ch == '\n') {
            x = x0;
            y += LINE_H;
            continue;
        }
        x += draw_glyph(x, y, ch, cur, 1) + 1;
        if (x > maxx)
            maxx = x;
    }
    return maxx - x0;
}

int text(int x, int y, const char *s, u16 c) { return text_n(x, y, s, 1 << 30, c); }

int text_w(const char *s)
{
    int w = 0, best = 0;
    for (int i = 0; s[i]; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == '^' && s[i + 1] >= '0' && s[i + 1] <= '9') { i++; continue; }
        if (ch == '\n') { w = 0; continue; }
        w += glyph_w(ch) + 1;
        if (w > best) best = w;
    }
    return best > 0 ? best - 1 : 0;
}

void text_sh(int x, int y, const char *s, u16 c, u16 shadow)
{
    text(x + 1, y + 1, s, shadow);
    text(x, y, s, c);
}

void text_center(int cx, int y, const char *s, u16 c, u16 shadow)
{
    text_sh(cx - text_w(s) / 2, y, s, c, shadow);
}

int text_big_w(const char *s, int scale)
{
    int w = 0;
    for (int i = 0; s[i]; i++)
        w += (glyph_w((unsigned char)s[i]) + 1) * scale;
    return w > 0 ? w - scale : 0;
}

void text_big(int x, int y, const char *s, int scale, u16 c, u16 shadow)
{
    for (int pass = 0; pass < 2; pass++) {
        int xx = x + (pass ? 0 : scale), yy = y + (pass ? 0 : scale);
        if (!pass && shadow == c)
            continue;
        for (int i = 0; s[i]; i++)
            xx += (draw_glyph(xx, yy, (unsigned char)s[i], pass ? c : shadow, scale) + 1) * scale;
    }
}

int wrap_text(const char *s, int maxw, int *starts, int *lens, int maxlines)
{
    int n = 0, i = 0;
    while (s[i] && n < maxlines) {
        int line_start = i, w = 0, last_space = -1, j = i;
        for (;;) {
            unsigned char ch = (unsigned char)s[j];
            if (!ch || ch == '\n')
                break;
            if (ch == '^' && s[j + 1] >= '0' && s[j + 1] <= '9') { j += 2; continue; }
            int gw = glyph_w(ch) + 1;
            if (w + gw - 1 > maxw && j > line_start) {
                if (last_space > line_start) j = last_space;
                break;
            }
            if (ch == ' ')
                last_space = j;
            w += gw;
            j++;
        }
        starts[n] = line_start;
        lens[n] = j - line_start;
        n++;
        i = j;
        while (s[i] == ' ')
            i++;
        if (s[i] == '\n')
            i++;
    }
    return n;
}

/* ---------------------------------------------------------------- math, random */

static u32 rng_state = 0x6E656B6Fu;   /* "neko" */

u32 rnd(void)
{
    u32 x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return rng_state = x;
}

void rnd_mix(u32 v)
{
    rng_state ^= v + 0x9E3779B9u + (rng_state << 6) + (rng_state >> 2);
    if (!rng_state)
        rng_state = 1;
}

int rnd_range(int lo, int hi)
{
    if (hi <= lo)
        return lo;
    return lo + (int)(rnd() % (u32)(hi - lo + 1));
}

static i16 sin_table[256];
static int sin_ready;

/* Quarter-wave sine in 1/256 units built with integer arithmetic (no floating point on the stick). */
static void sin_init(void)
{
    /* Bhaskara I: sin(x) ~ 16x(pi - x) / (5pi^2 - 4x(pi - x)), x in degrees form. */
    for (int i = 0; i < 256; i++) {
        int a = i & 127;                      /* 0..127 = 0..180 degrees */
        int deg = a * 180 / 128;
        int num = 4 * deg * (180 - deg);
        int den = 40500 - deg * (180 - deg);
        int v = num * 256 / den;
        sin_table[i] = (i16)(i < 128 ? v : -v);
    }
    sin_ready = 1;
}

int isin(int a)
{
    if (!sin_ready)
        sin_init();
    return sin_table[a & 255];
}

int icos(int a) { return isin(a + 64); }
int iabs(int v) { return v < 0 ? -v : v; }
int imin(int a, int b) { return a < b ? a : b; }
int imax(int a, int b) { return a > b ? a : b; }
int iclamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

u32 isqrt(u32 v)
{
    u32 r = 0, bit = 1u << 30;
    while (bit > v)
        bit >>= 2;
    while (bit) {
        if (v >= r + bit) {
            v -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return r;
}

int iatan2(int y, int x)
{
    /* Octant approximation, good to about a degree, result 0..255 per turn. */
    if (x == 0 && y == 0)
        return 0;
    int ax = iabs(x), ay = iabs(y), a;
    if (ax >= ay)
        a = ay * 32 / ax;         /* 0..32 over 0..45 degrees */
    else
        a = 64 - ax * 32 / ay;
    if (x < 0)
        a = 128 - a;
    if (y < 0)
        a = 256 - a;
    return a & 255;
}

/* ---------------------------------------------------------------- strings, memory */

int str_len(const char *s)
{
    int n = 0;
    while (s[n])
        n++;
    return n;
}

int str_eq(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

char *str_cpy(char *d, const char *s)
{
    while ((*d = *s++))
        d++;
    return d;
}

char *str_int(char *d, int v)
{
    char tmp[12];
    int n = 0;
    unsigned u = v < 0 ? (unsigned)-v : (unsigned)v;
    do { tmp[n++] = (char)('0' + u % 10); u /= 10; } while (u);
    if (v < 0) *d++ = '-';
    while (n) *d++ = tmp[--n];
    *d = 0;
    return d;
}

void mem_set(void *d, int v, u32 n)
{
    u8 *p = d;
    while (n--) *p++ = (u8)v;
}

void mem_copy(void *d, const void *s, u32 n)
{
    u8 *p = d;
    const u8 *q = s;
    while (n--) *p++ = *q++;
}

/* Filled triangle, scanline by scanline between its edges (integer only). */
void tri_fill(int x0, int y0, int x1, int y1, int x2, int y2, u16 c)
{
    int t;
    if (y1 < y0) { t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
    if (y2 < y0) { t = y0; y0 = y2; y2 = t; t = x0; x0 = x2; x2 = t; }
    if (y2 < y1) { t = y1; y1 = y2; y2 = t; t = x1; x1 = x2; x2 = t; }
    if (y2 == y0) {
        int lo = imin(x0, imin(x1, x2)), hi = imax(x0, imax(x1, x2));
        hline(lo, y0, hi - lo + 1, c);
        return;
    }
    for (int y = y0; y <= y2; y++) {
        int xa = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
        int xb = y < y1 ? x0 + (x1 - x0) * (y - y0) / (y1 - y0)
                        : (y2 == y1 ? x1 : x1 + (x2 - x1) * (y - y1) / (y2 - y1));
        if (xa > xb) { t = xa; xa = xb; xb = t; }
        hline(xa, y, xb - xa + 1, c);
    }
}

int text_wrapped(int x, int y, int w, const char *s, u16 c, int maxlines)
{
    int starts[8], lens[8];
    int n = wrap_text(s, w, starts, lens, imin(maxlines, 8));
    for (int i = 0; i < n; i++)
        text_n(x, y + i * LINE_H, s + starts[i], lens[i], c);
    return n;
}
