#include "dialog.h"
#include "cast.h"
#include "live.h"

#define QMAX 24
typedef struct {
    i8 who, voice;
    u8 expr, live;
    u16 color;
    const char *text, *name;
} Line;

static Line q[QMAX];
static LiveJob jobs[QMAX];     /* live lines, per queue slot */
static int q_head, q_count;
static int shown_x2;           /* characters revealed, times 2 (1.5 chars per frame) */
static int finished_flag, wait_t;
static char bufs[8][256];
static int buf_i;
int dlg_nlm_budget = 64;        /* most model steps per frame; time-sliced to ~4 ms where there is a clock */

char *dlg_buf(void)
{
    buf_i = (buf_i + 1) & 7;
    bufs[buf_i][0] = 0;
    return bufs[buf_i];
}

void dlg_clear(void)
{
    q_head = q_count = 0;
    shown_x2 = 0;
}

static Line *push_slot(void)
{
    if (q_count >= QMAX)
        return 0;
    Line *l = &q[(q_head + q_count) % QMAX];
    if (q_count == 0)
        shown_x2 = 0;
    q_count++;
    *l = (Line){ DLG_NARRATOR, 0, 0, 0, 0, "", 0 };
    return l;
}

void dlg_push(int who, int expr, const char *text)
{
    Line *l = push_slot();
    if (!l)
        return;
    l->who = (i8)who;
    l->expr = (u8)expr;
    l->text = text;
}

void dlg_push_named(const char *name, u16 color, int voice, const char *text)
{
    Line *l = push_slot();
    if (!l)
        return;
    l->who = DLG_NAMED;
    l->name = name;
    l->color = color;
    l->voice = (i8)voice;
    l->text = text;
}

static void make_live(Line *l, int spk, int sit, const char *key, const char *value)
{
    LiveJob *j = &jobs[l - q];
    live_init(j, spk, sit, key, value);
    l->live = 1;
    l->text = j->out;
}

void dlg_push_live(int who, int expr, int spk, int sit, const char *key, const char *value)
{
    Line *l = push_slot();
    if (!l)
        return;
    l->who = (i8)who;
    l->expr = (u8)expr;
    make_live(l, spk, sit, key, value);
}

void dlg_push_live_named(const char *name, u16 color, int voice, int spk, int sit, const char *key,
                         const char *value)
{
    Line *l = push_slot();
    if (!l)
        return;
    l->who = DLG_NAMED;
    l->name = name;
    l->color = color;
    l->voice = (i8)voice;
    make_live(l, spk, sit, key, value);
}

static int ready(const Line *l) { return !l->live || jobs[l - q].state == LIVE_DONE; }

int dlg_busy(void) { return q_count > 0; }
int dlg_just_finished(void) { return finished_flag; }
int dlg_who(void) { return q_count ? q[q_head].who : DLG_NARRATOR; }
int dlg_expr(void) { return q_count ? q[q_head].expr : 0; }

int dlg_wants_model(void)
{
    for (int i = 0; i < q_count; i++)
        if (!ready(&q[(q_head + i) % QMAX]))
            return 1;
    return 0;
}

static int visible_len(const char *s)
{
    int n = 0;
    for (int i = 0; s[i]; i++) {
        if (s[i] == '^' && s[i + 1] >= '0' && s[i + 1] <= '9') { i++; continue; }
        n++;
    }
    return n;
}

int dlg_typing(void)
{
    if (!q_count)
        return 0;
    return !ready(&q[q_head]) || shown_x2 / 2 < visible_len(q[q_head].text);
}

/* NekoLM writes the queued live lines in order, ahead of the one on screen. */
static void generate(void)
{
    for (int i = 0; i < q_count; i++) {
        Line *l = &q[(q_head + i) % QMAX];
        if (!ready(l)) {
            live_step(&jobs[l - q], 4000, dlg_nlm_budget);
            return;
        }
    }
}

static int voice_of(const Line *l)
{
    if (l->who >= 0)
        return cast[l->who].voice;
    return l->who == DLG_NAMED ? l->voice : -100;
}

void dlg_update(void)
{
    finished_flag = 0;
    generate();
    if (!q_count)
        return;
    const Line *l = &q[q_head];
    if (!ready(l)) {
        wait_t++;
        return;
    }
    wait_t = 0;
    int total = visible_len(l->text);
    if (shown_x2 / 2 < total) {
        int before = shown_x2 / 2;
        shown_x2 += 3;
        int now = shown_x2 / 2;
        /* one blip every other character, pitched per speaker */
        int v = voice_of(l);
        if (now / 2 != before / 2 && v > -100) {
            char ch = l->text[imin(now, str_len(l->text) - 1)];
            if (ch != ' ')
                sfx_voice(v + (int)(rnd() % 3));
        }
        if (any_pressed(BTN_A | BTN_B))
            shown_x2 = total * 2;
        return;
    }
    if (any_pressed(BTN_A | BTN_B)) {
        sfx(SFX_BLIP);
        q_head = (q_head + 1) % QMAX;
        q_count--;
        shown_x2 = 0;
        if (!q_count)
            finished_flag = 1;
    }
}

/* Reveal the first n visible characters: find the byte offset where they end. */
static int bytes_for(const char *s, int n)
{
    int i = 0, seen = 0;
    while (s[i] && seen < n) {
        if (s[i] == '^' && s[i + 1] >= '0' && s[i + 1] <= '9') { i += 2; continue; }
        i++;
        seen++;
    }
    return i;
}

void dlg_draw(int x, int y, int w, int h)
{
    if (!q_count)
        return;
    const Line *l = &q[q_head];
    round_box(x, y, w, h, C_WHITE, RGB(36, 30, 58));
    rect_blend(x + 2, y + 2, w - 4, 3, C_WHITE, 20);
    const char *nm = l->who >= 0 ? cast[l->who].name : l->who == DLG_NAMED ? l->name : 0;
    if (nm) {
        int nw = text_w(nm) + 10;
        round_box(x + 6, y - 7, nw, 12, C_WHITE, l->who >= 0 ? cast[l->who].name_color : l->color);
        text(x + 11, y - 5, nm, C_INK);
    }
    int ty = y + (nm ? 8 : 6);
    if (!ready(l)) {                               /* NekoLM is still writing this one */
        int dots = 1 + (int)(frame_count / 10) % 3;
        for (int i = 0; i < dots; i++)
            circle_fill(x + 12 + i * 7, ty + 4, 2, C_GREY);
        if (wait_t > 20)
            text(x + w - 60, y + h - 10, "\x02 thinking", C_GOLD);
        return;
    }
    int starts[6], lens[6];
    int n = wrap_text(l->text, w - 16, starts, lens, 6);
    int budget = bytes_for(l->text, shown_x2 / 2);
    for (int i = 0; i < n && ty + 8 < y + h; i++, ty += LINE_H) {
        int take = budget - starts[i];
        if (take <= 0)
            break;
        text_n(x + 8, ty, l->text + starts[i], imin(take, lens[i]), C_WHITE);
    }
    if (!dlg_typing() && (frame_count / 16) % 2 == 0)
        text(x + w - 12, y + h - 10, "\x06", C_PINK);
}
