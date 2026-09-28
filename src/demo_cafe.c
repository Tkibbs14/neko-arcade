/* Build 3 - Nyan Cafe: a rush-hour shift at Mocha's cafe, alone or with a friend on the second pad.
 * Five stations along the back (espresso, milk, tea, taiyaki grill, pastry case); demi-human customers
 * with their own tastes and patience sit at the counter. Everything they say is written live by NekoLM
 * (orders, thanks, grumbles, reviews) and Mocha keeps up a running commentary. */
#include "engine.h"
#include "save.h"
#include "cast.h"
#include "chibi.h"
#include "dialog.h"
#include "chatter.h"
#include "live.h"
#include "nekolm.h"
#include "hub.h"

#define COLS 5
#define SHIFT (60 * 120)
#define COL_X(i) (32 + (i) * 64)

enum { IT_NONE, IT_ESPRESSO, IT_LATTE, IT_MATCHA, IT_MATCHA_LATTE, IT_TAIYAKI, IT_CAKE, IT_MILK, IT_BURNT, IT_COUNT };
static const char *const item_names[IT_COUNT] = {
    "", "espresso", "latte", "matcha", "matcha latte", "taiyaki", "cake slice", "warm milk", "burnt taiyaki",
};
static const u8 item_price[IT_COUNT] = { 0, 3, 5, 4, 6, 4, 5, 2, 0 };

enum { SN_ESPRESSO, SN_MILK, SN_TEA, SN_GRILL, SN_PASTRY };
static const char *const station_names[COLS] = { "Espresso", "Milk", "Tea", "Taiyaki", "Pastry" };

static const struct {
    const char *name;
    int look, spk, voice, patience;
    u16 color;
    u8 likes[3];
} kinds[6] = {
    { "Tsuki", LK_TSUKI, SPK_CUST_SLEEPY, 2, 1750, RGB(150, 150, 206), { IT_ESPRESSO, IT_LATTE, IT_ESPRESSO } },
    { "Pipi", LK_PIPI, SPK_CUST_PERKY, 10, 1500, RGB(252, 172, 204), { IT_CAKE, IT_MATCHA_LATTE, IT_LATTE } },
    { "Mr. Whiskers", LK_WHISKERS, SPK_CUST_GRUMPY, -8, 1200, RGB(204, 180, 92), { IT_ESPRESSO, IT_TAIYAKI, IT_ESPRESSO } },
    { "Kitsu", LK_KITSU, SPK_CUST_FOX, 5, 1450, RGB(242, 142, 62), { IT_TAIYAKI, IT_CAKE, IT_MATCHA } },
    { "Hana", LK_HANA, SPK_CUST_DOG, 7, 1350, RGB(112, 172, 232), { IT_MILK, IT_CAKE, IT_LATTE } },
    { "Ponta", LK_PONTA, SPK_CUST_TANUKI, -3, 2100, RGB(172, 152, 112), { IT_TAIYAKI, IT_MATCHA, IT_MATCHA_LATTE } },
};

enum { C_EMPTY, C_ARRIVE, C_WAIT, C_EAT, C_LEAVE, C_STORM };
typedef struct { int kind, state, t, want, patience, pmax, expr, complained, flash; } Seat;
typedef struct { int busy, t; } Station;                 /* busy: 0 idle, 1 working, 2 ready, 3 burnt */
typedef struct { int col, x, hold, walk, moving, bump; } Barista;
typedef struct { int x, y, t, kind; char s[8]; } Floater;
typedef struct { int x, y, vx, vy, t, kind; } Puff;

enum { ST_TITLE, ST_INTRO, ST_PLAY, ST_CLOSING, ST_RESULTS };
static int state, state_t, players = 1, title_sel, paused;
static Seat seats[COLS];
static Station stations[COLS];
static Barista bar[2];
static Floater floaters[6];
static Puff puffs[24];
static int timer, next_spawn, earnings, served, walkouts, rush_said, stars;
static int visited[8], nvisited;                         /* customer kinds served this shift, for reviews */
static LiveJob reviews[3], closing;
static int review_kind[3], review_stars[3];

static void mocha(int sit, int prio)
{
    chat_say(cast[CH_MOCHA].name, cast[CH_MOCHA].name_color, cast[CH_MOCHA].voice, SPK_MOCHA, sit, 0, 0, prio);
}

static void customer_says(int col, int sit, int prio)
{
    const Seat *s = &seats[col];
    chat_say(kinds[s->kind].name, kinds[s->kind].color, kinds[s->kind].voice, kinds[s->kind].spk, sit,
             sit == SIT_O_ORDER ? "item" : 0, sit == SIT_O_ORDER ? item_names[s->want] : 0, prio);
}

static void floater(int x, int y, const char *s, int kind)
{
    for (int i = 0; i < 6; i++)
        if (floaters[i].t <= 0) {
            floaters[i] = (Floater){ x, y, 60, kind, { 0 } };
            str_cpy(floaters[i].s, s);
            return;
        }
}

static void puff(int x, int y, int kind)
{
    for (int i = 0; i < 24; i++)
        if (puffs[i].t <= 0) {
            puffs[i] = (Puff){ x * 16, y * 16, rnd_range(-6, 6), rnd_range(-14, -8), rnd_range(30, 50), kind };
            return;
        }
}

/* ---------------------------------------------------------------- items */
static void draw_item(int it, int x, int y)       /* 12x12 icon, top-left at x,y */
{
    u16 ink = RGB(58, 40, 62);
    switch (it) {
    case IT_ESPRESSO:
        hline(x + 1, y + 11, 10, ink);
        round_box(x + 2, y + 5, 8, 6, ink, C_WHITE);
        rect(x + 3, y + 6, 6, 2, RGB(110, 60, 40));
        pset(x + 10, y + 7, ink); pset(x + 10, y + 8, ink);
        pset(x + 5, y + 2, C_GREY); pset(x + 6, y + 3, C_GREY);
        break;
    case IT_LATTE:
    case IT_MATCHA_LATTE:
        round_box(x + 3, y + 1, 7, 11, ink, it == IT_LATTE ? RGB(200, 150, 100) : RGB(150, 200, 110));
        rect(x + 4, y + 2, 5, 3, C_WHITE);
        rect(x + 4, y + 8, 5, 3, it == IT_LATTE ? RGB(150, 96, 60) : C_WHITE);
        pset(x + 6, y + 3, C_PINK);
        break;
    case IT_MATCHA:
        ellipse_fill(x + 6, y + 8, 6, 4, ink);
        ellipse_fill(x + 6, y + 8, 5, 3, RGB(90, 140, 80));
        ellipse_fill(x + 6, y + 7, 4, 1, RGB(170, 220, 120));
        break;
    case IT_TAIYAKI:
    case IT_BURNT: {
        u16 body = it == IT_TAIYAKI ? RGB(232, 170, 80) : RGB(70, 56, 52);
        tri_fill(x + 9, y + 6, x + 12, y + 3, x + 12, y + 9, ink);
        ellipse_fill(x + 5, y + 6, 5, 4, ink);
        ellipse_fill(x + 5, y + 6, 4, 3, body);
        tri_fill(x + 9, y + 6, x + 11, y + 4, x + 11, y + 8, body);
        pset(x + 3, y + 5, ink);
        pset(x + 6, y + 7, blend(body, ink, 90)); pset(x + 7, y + 5, blend(body, ink, 90));
        break;
    }
    case IT_CAKE:
        tri_fill(x + 1, y + 11, x + 11, y + 11, x + 11, y + 4, ink);
        tri_fill(x + 2, y + 10, x + 10, y + 10, x + 10, y + 5, RGB(255, 236, 214));
        hline(x + 5, y + 8, 5, RGB(255, 170, 190));
        circle_fill(x + 9, y + 3, 2, C_RED);
        break;
    case IT_MILK:
        round_box(x + 3, y + 1, 7, 11, ink, C_WHITE);
        rect(x + 4, y + 2, 5, 2, RGB(214, 228, 244));
        pset(x + 5, y + 7, C_PINK); pset(x + 7, y + 7, C_PINK); pset(x + 6, y + 8, C_PINK);
        break;
    }
}

/* ---------------------------------------------------------------- the room */
static void draw_room(void)
{
    gradient_v(0, 12, SCREEN_W, 64, RGB(252, 228, 200), RGB(238, 204, 172));
    /* bunting and a shelf of jars above the stations */
    for (int i = 0; i < 20; i++) {
        static const u16 flags[4] = { C_PINK, C_GOLD, C_MINT, C_SKY };
        int x = i * 16 + 2;
        tri_fill(x, 38, x + 10, 38, x + 5, 45, flags[i % 4]);
    }
    hline(0, 38, SCREEN_W, RGB(150, 100, 70));
    /* back counter and floor */
    rect(0, 74, SCREEN_W, 4, RGB(214, 168, 120));
    rect(0, 78, SCREEN_W, 10, RGB(150, 100, 70));
    for (int i = 0; i < COLS; i++)
        text_center(COL_X(i), 79, station_names[i], RGB(250, 230, 200), RGB(110, 70, 50));
    rect(0, 88, SCREEN_W, 22, RGB(176, 150, 124));
    for (int y = 88; y < 110; y += 8)
        for (int x = ((y - 88) / 8) % 2 * 8; x < SCREEN_W; x += 16)
            rect(x, y, 8, imin(8, 110 - y), RGB(236, 222, 204));
    rect_blend(0, 88, SCREEN_W, 22, RGB(200, 170, 140), 60);
}

static void draw_front_counter(void)
{
    rect(0, 110, SCREEN_W, 3, RGB(226, 180, 130));
    rect(0, 113, SCREEN_W, 11, RGB(170, 116, 80));
    for (int x = 10; x < SCREEN_W; x += 20)
        vline(x, 114, 10, RGB(146, 98, 68));
    /* customer side: warm tiles and stools */
    gradient_v(0, 124, SCREEN_W, 56, RGB(214, 170, 132), RGB(180, 132, 100));
    for (int i = 0; i < COLS; i++) {
        ellipse_fill(COL_X(i), 172, 14, 4, RGB(120, 80, 60));
        ellipse_fill(COL_X(i), 170, 13, 3, RGB(232, 120, 150));
    }
}

static void draw_station(int i)
{
    int cx = COL_X(i);
    const Station *s = &stations[i];
    u16 ink = RGB(58, 40, 62);
    switch (i) {
    case SN_ESPRESSO:
        round_box(cx - 14, 50, 28, 24, ink, RGB(200, 206, 216));
        rect(cx - 12, 52, 24, 4, RGB(150, 156, 170));
        circle_fill(cx + 7, 60, 3, C_WHITE);
        circle_line(cx + 7, 60, 3, ink);
        pset(cx + 7 + (s->busy == 1 ? (int)(frame_count / 4) % 3 - 1 : 0), 59, C_RED);
        rect(cx - 7, 58, 6, 3, RGB(70, 70, 84));
        if (s->busy)
            draw_item(IT_ESPRESSO, cx - 10, 62);
        break;
    case SN_MILK:
        round_box(cx - 10, 52, 16, 22, ink, C_WHITE);
        rect(cx - 9, 58, 14, 3, RGB(106, 168, 255));
        round_box(cx + 7, 62, 8, 12, ink, RGB(214, 228, 244));
        break;
    case SN_TEA:
        rect(cx - 12, 66, 24, 8, RGB(80, 80, 96));
        circle_fill(cx - 3, 60, 7, ink);
        circle_fill(cx - 3, 60, 6, RGB(60, 110, 90));
        line(cx + 3, 58, cx + 8, 54, ink);
        rect(cx - 5, 52, 4, 2, ink);
        if (s->busy == 2)
            draw_item(IT_MATCHA, cx + 2, 62);
        break;
    case SN_GRILL:
        round_box(cx - 15, 60, 30, 14, ink, RGB(70, 66, 76));
        for (int k = 0; k < 2; k++) {
            int fx = cx - 13 + k * 14;
            if (s->busy)
                draw_item(s->busy == 3 ? IT_BURNT : IT_TAIYAKI, fx, 61);
            else
                ellipse_fill(fx + 5, 67, 5, 3, RGB(50, 46, 56));
        }
        if (s->busy == 1)                          /* raw batter still pale */
            rect_blend(cx - 14, 61, 28, 12, RGB(250, 230, 190), 150 - imin(150, s->t * 150 / 110));
        break;
    case SN_PASTRY:
        round_box(cx - 16, 52, 32, 22, ink, RGB(214, 236, 250));
        rect_blend(cx - 15, 53, 30, 20, C_WHITE, 60);
        hline(cx - 15, 63, 30, ink);
        draw_item(IT_CAKE, cx - 14, 51);
        draw_item(IT_CAKE, cx + 1, 51);
        draw_item(IT_CAKE, cx - 7, 62);
        break;
    }
    /* progress: a ring of dots while working, a bouncing star when ready */
    if (s->busy == 1) {
        int need = i == SN_GRILL ? 110 : i == SN_TEA ? 80 : 70;
        int w = s->t * 20 / need;
        rect(cx - 10, 46, 20, 3, ink);
        rect(cx - 10, 46, imin(w, 20), 3, C_MINT);
    } else if (s->busy == 2 && i != SN_MILK && i != SN_PASTRY) {
        text(cx - 2, 42 + isin((int)frame_count * 8) / 128, "\x02", C_GOLD);
    } else if (s->busy == 3) {
        text(cx - 2, 42, "!", C_RED);
    }
}

/* ---------------------------------------------------------------- stations and serving */
static void take(Barista *b, int it)
{
    b->hold = it;
    sfx(SFX_OK);
}

static void interact(Barista *b, int pl)
{
    int c = b->col;
    Seat *s = &seats[c];
    Station *st = &stations[c];
    if (b->hold && s->state == C_WAIT && s->want == b->hold) {                    /* serve */
        int tip = 1 + 5 * s->patience / s->pmax, coins = item_price[b->hold] + tip;
        earnings += coins;
        served++;
        char buf[8];
        str_int(str_cpy(buf, "+$"), coins);
        floater(COL_X(c), 128, buf, 0);
        sfx(SFX_COIN);
        if (nvisited < 8) visited[nvisited++] = s->kind;
        s->state = C_EAT;
        s->t = 0;
        s->expr = CE_HAPPY;
        b->hold = IT_NONE;
        customer_says(c, SIT_O_THANKS, 1);
        if (tip >= 5 && rnd_range(0, 1)) mocha(SIT_C_TIP, 1);
        else if (s->patience * 10 > s->pmax * 7 && rnd_range(0, 2) == 0) mocha(SIT_C_GOOD, 1);
        return;
    }
    switch (c) {
    case SN_ESPRESSO:
    case SN_TEA:
        if (st->busy == 0) { st->busy = 1; st->t = 0; sfx(SFX_BLIP); return; }
        if (st->busy == 2 && !b->hold) { take(b, c == SN_ESPRESSO ? IT_ESPRESSO : IT_MATCHA); st->busy = 0; return; }
        break;
    case SN_MILK:
        if (b->hold == IT_ESPRESSO) { take(b, IT_LATTE); return; }
        if (b->hold == IT_MATCHA) { take(b, IT_MATCHA_LATTE); return; }
        if (!b->hold) { take(b, IT_MILK); return; }
        break;
    case SN_GRILL:
        if (st->busy == 0) { st->busy = 1; st->t = 0; sfx(SFX_SPLASH); return; }
        if (st->busy >= 2 && !b->hold) { take(b, st->busy == 3 ? IT_BURNT : IT_TAIYAKI); st->busy = 0; return; }
        break;
    case SN_PASTRY:
        if (!b->hold) { take(b, IT_CAKE); return; }
        break;
    }
    if (b->hold && s->state == C_WAIT) {                                          /* wrong order */
        s->flash = 30;
        s->expr = CE_SURPRISED;
        s->patience -= s->pmax / 7;
        sfx(SFX_BUZZ);
        return;
    }
    b->bump = 8;
    (void)pl;
}

static void spawn(void)
{
    int free[COLS], n = 0;
    for (int i = 0; i < COLS; i++)
        if (seats[i].state == C_EMPTY) free[n++] = i;
    if (!n)
        return;
    Seat *s = &seats[free[rnd_range(0, n - 1)]];
    s->kind = rnd_range(0, 5);
    s->want = rnd_range(0, 9) < 7 ? kinds[s->kind].likes[rnd_range(0, 2)] : rnd_range(IT_ESPRESSO, IT_MILK);
    s->pmax = s->patience = kinds[s->kind].patience * (players == 2 ? 3 : 4) / 4;
    s->state = C_ARRIVE;
    s->t = 0;
    s->expr = kinds[s->kind].look == LK_TSUKI ? CE_SLEEPY : CE_NORMAL;
    s->complained = 0;
    s->flash = 0;
    sfx(SFX_DING);
}

static void update_seats(void)
{
    int waiting = 0;
    for (int i = 0; i < COLS; i++) {
        Seat *s = &seats[i];
        s->t++;
        if (s->flash) s->flash--;
        switch (s->state) {
        case C_ARRIVE:
            if (s->t == 24) {
                s->state = C_WAIT;
                customer_says(i, SIT_O_ORDER, 2);
            }
            break;
        case C_WAIT:
            waiting++;
            if (state == ST_PLAY) s->patience--;
            if (!s->flash)
                s->expr = s->patience * 4 < s->pmax ? CE_ANGRY : kinds[s->kind].look == LK_TSUKI ? CE_SLEEPY : CE_NORMAL;
            if (!s->complained && s->patience * 10 < s->pmax * 3) {
                s->complained = 1;
                customer_says(i, SIT_O_WAIT, 1);
                if (rnd_range(0, 1)) mocha(SIT_C_SLOW, 1);
            }
            if (s->patience <= 0) {
                s->state = C_STORM;
                s->t = 0;
                s->expr = CE_ANGRY;
                walkouts++;
                sfx(SFX_WHOOSH);
                customer_says(i, SIT_O_LEAVE, 3);
                mocha(SIT_C_WALKOUT, 1);
            }
            break;
        case C_EAT:
            if (s->t % 20 == 0) puff(COL_X(i) + rnd_range(-8, 8), 128, 1);
            if (s->t > 100) { s->state = C_LEAVE; s->t = 0; }
            break;
        case C_LEAVE:
        case C_STORM:
            if (s->t > 30) s->state = C_EMPTY;
            break;
        }
    }
    if (waiting >= 3 && timer - rush_said > 60 * 20 && state == ST_PLAY) {
        rush_said = timer;
        mocha(SIT_C_RUSH, 1);
    }
}

static void update_stations(void)
{
    for (int i = 0; i < COLS; i++) {
        Station *s = &stations[i];
        if (!s->busy)
            continue;
        s->t++;
        if (i == SN_ESPRESSO || i == SN_TEA) {
            int need = i == SN_TEA ? 80 : 70;
            if (s->busy == 1 && s->t % 12 == 0) puff(COL_X(i) - 4, 50, 0);
            if (s->busy == 1 && s->t >= need) { s->busy = 2; sfx(SFX_DING); }
        } else if (i == SN_GRILL) {
            if (s->busy == 1 && s->t >= 110) { s->busy = 2; sfx(SFX_DING); }
            if (s->busy == 2 && s->t >= 110 + 330) { s->busy = 3; sfx(SFX_BUZZ); }
            if (s->busy == 3 && s->t % 8 == 0) puff(COL_X(i) + rnd_range(-10, 10), 58, 2);
        }
    }
}

static void update_baristas(void)
{
    for (int p = 0; p < players; p++) {
        Barista *b = &bar[p];
        if (btn_repeat(p, BTN_LEFT) && b->col > 0) b->col--;
        if (btn_repeat(p, BTN_RIGHT) && b->col < COLS - 1) b->col++;
        int tx = COL_X(b->col) + (players == 2 ? (p ? 7 : -7) : 0), d = tx - b->x;
        b->moving = d != 0;
        b->x += d > 4 ? 4 : d < -4 ? -4 : d;
        if (b->moving) b->walk++;
        if (b->bump) b->bump--;
        if (btn_pressed(p, BTN_A) && !b->moving) interact(b, p);
        if (btn_pressed(p, BTN_B) && b->hold) {
            b->hold = IT_NONE;
            sfx(SFX_POP);
            puff(b->x, 96, 2);
        }
    }
}

/* ---------------------------------------------------------------- flow */
static void start_shift(void)
{
    for (int i = 0; i < COLS; i++) {
        seats[i] = (Seat){ 0 };
        stations[i] = (Station){ 0, 0 };
    }
    for (int p = 0; p < 2; p++)
        bar[p] = (Barista){ p ? 3 : 1, COL_X(p ? 3 : 1), IT_NONE, 0, 0, 0 };
    for (int i = 0; i < 6; i++) floaters[i].t = 0;
    for (int i = 0; i < 24; i++) puffs[i].t = 0;
    timer = SHIFT;
    paused = 0;
    next_spawn = 60;
    earnings = served = walkouts = 0;
    rush_said = SHIFT + 60 * 30;
    nvisited = 0;
    chat_clear();
    dlg_clear();
    dlg_push_live(CH_MOCHA, EX_HAPPY, SPK_MOCHA, SIT_C_OPEN, 0, 0);
    dlg_push(CH_MOCHA, EX_NEUTRAL, "Stations are along the back. ^3A^0 starts a machine or grabs what's ready, "
                                   "^3B^0 bins what you carry.");
    dlg_push(CH_MOCHA, EX_SMUG, "Espresso or matcha plus ^3Milk^0 makes a latte. Taiyaki burns if you "
                                "forget it!");
    dlg_push(CH_MOCHA, EX_SPARKLE, "Serve what's in the bubble. The faster you are, the bigger the tip.");
    state = ST_INTRO;
    state_t = 0;
}

static void finish_shift(void)
{
    int bar3 = players == 2 ? 150 : 110, bar2 = bar3 * 2 / 3, bar1 = bar3 / 3;
    stars = earnings >= bar3 ? 3 : earnings >= bar2 ? 2 : earnings >= bar1 ? 1 : 0;
    save.cafe_runs++;
    if (earnings > save.cafe_best) save.cafe_best = (u16)earnings;
    if (stars > save.cafe_stars) save.cafe_stars = (u8)stars;
    hub_report(SIT_AFTER_CAFE);
    {
        char *b = log_buf(), *q = str_cpy(b, "cafe: shift $");
        q = str_int(q, earnings);
        q = str_cpy(q, ", served ");
        q = str_int(q, served);
        q = str_cpy(q, ", walkouts ");
        q = str_int(q, walkouts);
        q = str_cpy(q, ", stars ");
        q = str_int(q, stars);
        q = str_cpy(q, "/3, baristas ");
        str_int(q, players);
        platform_log(b);
    }
    /* three reviews from customers who came in, and Mocha's closing line */
    for (int r = 0; r < 3; r++) {
        int k = nvisited ? visited[rnd_range(0, nvisited - 1)] : rnd_range(0, 5);
        int st = stars == 3 ? (r < 2 ? 5 : 3) : stars == 2 ? (r == 0 ? 5 : 3) : stars == 1 ? (r == 0 ? 3 : 1) : 1;
        if (r == 2 && walkouts > 2) st = 1;
        review_kind[r] = k;
        review_stars[r] = st;
        live_init(&reviews[r], kinds[k].spk, st == 5 ? SIT_O_REVIEW5 : st == 3 ? SIT_O_REVIEW3 : SIT_O_REVIEW1, 0, 0);
    }
    live_init(&closing, SPK_MOCHA, stars >= 2 ? SIT_C_CLOSE_GOOD : SIT_C_CLOSE_BAD, 0, 0);
    state = ST_RESULTS;
    state_t = 0;
    sfx(stars >= 2 ? SFX_GOAL : SFX_LOSE);
}

static void cafe_enter(void)
{
    music(MUS_CAFE);
    live_ban_he = 0;
    chat_clear();
    dlg_clear();
    state = ST_TITLE;
    state_t = 0;
}

static void cafe_update(void)
{
    state_t++;
    for (int i = 0; i < 6; i++) if (floaters[i].t > 0) { floaters[i].t--; floaters[i].y -= (floaters[i].t & 1); }
    for (int i = 0; i < 24; i++) if (puffs[i].t > 0) { puffs[i].t--; puffs[i].x += puffs[i].vx; puffs[i].y += puffs[i].vy; }
    switch (state) {
    case ST_TITLE:
        if (any_repeat(BTN_UP | BTN_DOWN)) { title_sel ^= 1; sfx(SFX_MOVE); }
        if (any_pressed(BTN_A)) { players = title_sel + 1; sfx(SFX_OK); start_shift(); }
        if (any_pressed(BTN_B)) { sfx(SFX_BACK); scene_fade_to(&scene_menu); }
        break;
    case ST_INTRO:
        dlg_update();
        if (dlg_just_finished()) { state = ST_PLAY; state_t = 0; sfx(SFX_GOAL); }
        break;
    case ST_PLAY:
        if (paused) {
            if (any_pressed(BTN_A | BTN_START)) { paused = 0; sfx(SFX_OK); }
            else if (any_pressed(BTN_B)) { paused = 0; sfx(SFX_BACK); chat_clear(); state = ST_TITLE; state_t = 0; }
            break;
        }
        if (any_pressed(BTN_START)) { paused = 1; sfx(SFX_POP); break; }
        chat_update();
        update_baristas();
        update_stations();
        update_seats();
        if (--next_spawn <= 0) {
            spawn();
            int fast = timer < SHIFT / 2;
            next_spawn = players == 2 ? rnd_range(fast ? 110 : 170, fast ? 230 : 330)
                                      : rnd_range(fast ? 170 : 250, fast ? 330 : 470);
        }
        if (--timer <= 0) {
            state = ST_CLOSING;
            state_t = 0;
            sfx(SFX_DING);
        }
        break;
    case ST_CLOSING:                                   /* doors shut; whoever is left finishes up */
        chat_update();
        update_stations();
        update_seats();
        for (int i = 0; i < COLS; i++)
            if (seats[i].state == C_WAIT || seats[i].state == C_ARRIVE) { seats[i].state = C_LEAVE; seats[i].t = 0; }
        if (state_t > 120)
            finish_shift();
        break;
    case ST_RESULTS: {
        int done = 1;                                  /* write the reviews one after another */
        for (int r = 0; r < 3 && done; r++)
            if (!live_step(&reviews[r], 4000, 64)) done = 0;
        if (done) live_step(&closing, 4000, 64);
        if (state_t > 60 && any_pressed(BTN_A)) { sfx(SFX_OK); state = ST_TITLE; state_t = 0; }
        else if (state_t > 60 && any_pressed(BTN_B)) { sfx(SFX_BACK); scene_fade_to(&scene_menu); }
        break;
    }
    }
}

/* ---------------------------------------------------------------- draw */
static void draw_seat(int i)
{
    const Seat *s = &seats[i];
    if (s->state == C_EMPTY)
        return;
    int cx = COL_X(i) + (s->flash ? isin((int)frame_count * 64) / 100 : 0), cy = 142;
    if (s->state == C_ARRIVE) cy += (24 - s->t) / 2;
    if (s->state == C_LEAVE || s->state == C_STORM) cy += s->t / 2;
    chibi_bust(kinds[s->kind].look, cx, cy, s->expr);
    if (s->state == C_STORM && s->t < 24)
        text(cx + 10, cy - 18, "#", C_RED);
    if (s->state != C_WAIT)
        return;
    /* the order bubble and the patience bar */
    int bx = cx + 11, by = 124;
    round_box(bx, by, 18, 17, RGB(58, 40, 62), C_WHITE);
    pset(bx - 1, by + 10, C_WHITE); pset(bx - 2, by + 11, RGB(58, 40, 62));
    draw_item(s->want, bx + 3, by + 3);
    int w = s->patience * 18 / s->pmax;
    u16 pc = s->patience * 2 > s->pmax ? C_MINT : s->patience * 4 > s->pmax ? C_GOLD : C_RED;
    rect(bx, by + 19, 18, 3, RGB(58, 40, 62));
    rect(bx, by + 19, imax(1, w), 3, pc);
}

static void draw_hud(void)
{
    rect(0, 0, SCREEN_W, 12, C_INK);
    text(4, 2, "\x03 NYAN CAFE", C_ORANGE);
    int sec = (timer + 59) / 60;
    char buf[24], *p = buf;
    p = str_int(p, sec / 60);
    p = str_cpy(p, sec % 60 < 10 ? ":0" : ":");
    str_int(p, sec % 60);
    text_center(SCREEN_W / 2, 2, buf, sec <= 10 && (frame_count / 15) % 2 ? C_RED : C_WHITE, C_INK);
    p = str_cpy(buf, "$");
    str_int(p, earnings);
    text(SCREEN_W - 40, 2, buf, C_GOLD);
    p = str_cpy(buf, "\x01");
    str_int(p, served);
    text(SCREEN_W - 78, 2, buf, C_PINK);
}

static void draw_play(void)
{
    draw_room();
    for (int i = 0; i < COLS; i++)
        draw_station(i);
    for (int p = 0; p < players; p++) {
        const Barista *b = &bar[p];
        int by = 88 - (b->bump ? (b->bump > 4 ? 2 : 1) : 0);
        chibi(p ? LK_HIRE2 : LK_HIRE, b->x, by, b->hold == IT_BURNT ? CE_SAD : CE_NORMAL,
              b->moving ? 1 + (b->walk / 5) % 3 : 0, p);
        if (b->hold) {
            round_box(b->x + 6, by - 2, 16, 16, RGB(58, 40, 62), C_WHITE);
            draw_item(b->hold, b->x + 8, by);
        }
        if (players == 2)
            text(b->x - 3, by - 20, p ? "P2" : "P1", p ? C_PINK : C_MINT);
    }
    draw_front_counter();
    for (int i = 0; i < COLS; i++)
        draw_seat(i);
    for (int i = 0; i < 24; i++) {
        const Puff *q = &puffs[i];
        if (q->t <= 0) continue;
        int x = q->x / 16, y = q->y / 16;
        if (q->kind == 1) text(x - 2, y, "\x01", C_PINK);
        else circle_fill(x, y, q->t > 20 ? 2 : 1, q->kind == 2 ? RGB(90, 84, 96) : RGB(250, 250, 250));
    }
    for (int i = 0; i < 6; i++)
        if (floaters[i].t > 0)
            text_center(floaters[i].x, floaters[i].y, floaters[i].s, C_GOLD, C_INK);
    draw_hud();
    rect(0, 12, SCREEN_W, 24, blend(C_INK, RGB(252, 228, 200), 40));
    chat_draw(4, 14, 312);
}

static void draw_stars(int cx, int y, int n, int of)
{
    for (int i = 0; i < of; i++)
        text(cx - of * 5 + i * 10, y, "\x02", i < n ? C_GOLD : C_DIM);
}

static void draw_results(void)
{
    draw_play();
    rect_blend(0, 0, SCREEN_W, SCREEN_H, C_NIGHT, 190);
    round_box(6, 4, 308, 174, C_ORANGE, RGB(44, 34, 56));
    text_big(14, 9, "SHIFT OVER", 2, C_ORANGE, C_INK);
    for (int i = 0; i < 3; i++)
        text_big(150 + i * 22, 6, "\x02", 2, i < stars ? C_GOLD : C_PANEL2, C_INK);
    text(226, 8, "^3A^0 again", C_GREY);
    text(226, 18, "^3B^0 back to Nia", C_GREY);
    char buf[48], *p = buf;
    p = str_cpy(p, "$");
    p = str_int(p, earnings);
    p = str_cpy(p, " earned   ");
    p = str_int(p, served);
    p = str_cpy(p, " served   ");
    p = str_int(p, walkouts);
    str_cpy(p, walkouts == 1 ? " walkout" : " walkouts");
    text(14, 30, buf, C_WHITE);
    for (int r = 0; r < 3; r++) {
        int y = 42 + r * 34;
        round_box(10, y, 300, 32, C_DIM, RGB(58, 48, 74));
        chibi_head(kinds[review_kind[r]].look, 24, y + 17, review_stars[r] >= 3 ? CE_HAPPY : CE_SAD);
        text(40, y + 2, kinds[review_kind[r]].name, kinds[review_kind[r]].color);
        draw_stars(40 + text_w(kinds[review_kind[r]].name) + 30, y + 2, review_stars[r], 5);
        if (reviews[r].state == LIVE_DONE)
            text_wrapped(40, y + 12, 264, reviews[r].out, C_WHITE, 2);
        else
            text(40, y + 12, "writing a review...", C_DIM);
    }
    if (closing.state == LIVE_DONE) {
        round_box(10, 146, 300, 28, C_WHITE, RGB(250, 236, 220));
        text(16, 149, "Mocha", cast[CH_MOCHA].name_color);
        text_wrapped(48, 149, 256, closing.out, C_INK, 2);
    }
}

static void draw_title(void)
{
    draw_room();
    for (int i = 0; i < COLS; i++)
        draw_station(i);
    draw_front_counter();
    rect_blend(0, 0, SCREEN_W, SCREEN_H, C_NIGHT, 170);
    rect_blend(4, 10, 180, 166, C_NIGHT, 120);
    draw_bust(CH_MOCHA, EX_HAPPY, 190, 40, 2);
    text_big(14, 18, "NYAN", 3, C_ORANGE, C_INK);
    text_big(14, 44, "CAFE", 3, C_WHITE, C_INK);
    static const char *const opts[2] = { "1 barista", "2 baristas (pad 2 joins)" };
    for (int i = 0; i < 2; i++) {
        int y = 88 + i * 18;
        if (i == title_sel) {
            round_box(10, y - 3, 170, 15, C_ORANGE, C_PANEL2);
            spr(SPR_PAW, 14, y - 1, 0);
        }
        text_sh(30, y, opts[i], i == title_sel ? C_WHITE : C_GREY, C_INK);
    }
    char buf[40], *p = buf;
    p = str_cpy(p, "best $");
    p = str_int(p, save.cafe_best);
    str_cpy(p, "   shifts ");
    str_int(buf + str_len(buf), save.cafe_runs);
    text_sh(14, 132, buf, C_GOLD, C_INK);
    draw_stars(60, 144, save.cafe_stars, 3);
    text(10, 166, "^3A^0 open the cafe   ^3B^0 back to Nia", C_GREY);
}

static void cafe_draw(void)
{
    switch (state) {
    case ST_TITLE:
        draw_title();
        break;
    case ST_INTRO:
        draw_play();
        rect_blend(0, 36, SCREEN_W, SCREEN_H - 36, C_NIGHT, 90);
        draw_bust(CH_MOCHA, dlg_expr(), 250, 60, 1);
        dlg_draw(4, 126, 312, 50);
        break;
    case ST_PLAY:
        draw_play();
        if (state_t < 60)
            text_big(SCREEN_W / 2 - text_big_w("OPEN!", 3) / 2, 96, "OPEN!", 3, C_ORANGE, C_INK);
        if (paused) {
            rect_blend(0, 0, SCREEN_W, SCREEN_H, C_NIGHT, 160);
            round_box(80, 66, 160, 44, C_ORANGE, C_PANEL);
            text_center(SCREEN_W / 2, 74, "Break time", C_ORANGE, C_INK);
            text_center(SCREEN_W / 2, 92, "^3A^0 back to work   ^3B^0 leave", C_WHITE, C_INK);
        }
        break;
    case ST_CLOSING:
        draw_play();
        text_big(SCREEN_W / 2 - text_big_w("CLOSING TIME", 2) / 2, 92, "CLOSING TIME", 2, C_ORANGE, C_INK);
        break;
    case ST_RESULTS:
        draw_results();
        break;
    }
}

const Scene scene_cafe = { cafe_enter, cafe_update, cafe_draw, "cafe" };
