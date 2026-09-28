#include "chatter.h"
#include "live.h"
#include "dialog.h"

#define WAITING 5
#define MIN_SHOW 150            /* frames a line stays before the next one may replace it */
#define FADE_AT 330

typedef struct { const char *name; u16 color; i8 voice; u8 prio; LiveJob job; } Req;

static Req waiting[WAITING];
static int nwait;
static const char *shown_name;
static u16 shown_color;
static char shown_text[128];
static int shown_age = 9999, reveal;

void chat_clear(void)
{
    nwait = 0;
    shown_text[0] = 0;
    shown_age = 9999;
}

int chat_idle(void) { return nwait == 0; }
int chat_showing(void) { return shown_text[0] && shown_age <= FADE_AT; }

void chat_say(const char *name, u16 color, int voice, int spk, int sit, const char *key, const char *value,
              int prio)
{
    if (nwait == WAITING) {
        int lo = 0;
        for (int i = 1; i < WAITING; i++)
            if (waiting[i].prio < waiting[lo].prio) lo = i;
        if (waiting[lo].prio > prio)
            return;
        for (int i = lo; i < WAITING - 1; i++) waiting[i] = waiting[i + 1];
        nwait--;
    }
    Req *r = &waiting[nwait++];
    r->name = name;
    r->color = color;
    r->voice = (i8)voice;
    r->prio = (u8)prio;
    live_init(&r->job, spk, sit, key, value);
}

void chat_update(void)
{
    shown_age++;
    reveal += 2;
    if (!nwait || dlg_wants_model())
        return;
    if (!live_step(&waiting[0].job, 3000, 64))
        return;
    if (shown_text[0] && shown_age < MIN_SHOW)
        return;                                    /* written; waits until the current line has been read */
    shown_name = waiting[0].name;
    shown_color = waiting[0].color;
    str_cpy(shown_text, waiting[0].job.out);
    shown_age = 0;
    reveal = 0;
    sfx_voice(waiting[0].voice);
    for (int i = 0; i < nwait - 1; i++) waiting[i] = waiting[i + 1];
    nwait--;
}

void chat_draw(int x, int y, int w)
{
    if (!shown_text[0] || shown_age > FADE_AT)
        return;
    int tw = text_w(shown_name) + 10;
    u16 ink = shown_age > FADE_AT - 40 ? C_DIM : C_WHITE;
    round_box(x, y, tw, 11, C_WHITE, shown_color);
    text(x + 5, y + 2, shown_name, C_INK);
    int starts[2], lens[2];
    int n = wrap_text(shown_text, w - tw - 6, starts, lens, 2);
    int left = reveal;
    for (int i = 0; i < n && left > 0; i++) {
        text_n(x + tw + 5, y + 2 + i * LINE_H, shown_text + starts[i], imin(lens[i], left), ink);
        left -= lens[i];
    }
}
