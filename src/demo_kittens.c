/* Build 4 - Foster Kittens: Nia's three foster kittens, each with a tiny neural brain: a one-layer
 * softmax policy (6 behaviours x 4 inputs = 24 weights) from hunger, tiredness and boredom to what the
 * kitten does next. Praise and "no!" are real policy-gradient updates on that brain, so each kitten learns
 * the house rules from you (scratching post yes, couch no, cups stay on the table) and remembers them in
 * the save. Their thoughts are written live by NekoLM. */
#include "engine.h"
#include "save.h"
#include "cast.h"
#include "dialog.h"
#include "chatter.h"
#include "live.h"
#include "nekolm.h"

#define NK 3
#define NA 7                     /* 6 learned behaviours + sitting around (fixed score 0) */
#define NF 4                     /* inputs: bias, hunger, tiredness, boredom */

enum { A_EAT, A_NAP, A_PLAY, A_POST, A_COUCH, A_KNOCK, A_IDLE };
static const char *const act_names[NA] = { "eat", "nap", "yarn", "post", "couch", "cup", "sit" };
static const u8 act_naughty[NA] = { 0, 0, 0, 0, 1, 1, 0 };

enum { K_SIT, K_WALK, K_DO, K_CHASE, K_JUMP_UP, K_ON_TABLE, K_JUMP_DOWN, K_FLAT, K_PURR };

typedef struct {
    int x, y, tx, ty, dir, state, t, act, act_t, last_act, last_t, fx, bob, thought_t;
    int hunger, tired, bored;    /* 0..255 */
} Kitten;

static const struct { const char *name; u16 fur, dark, belly, eye; int voice; } coats[NK] = {
    { "Mochi", RGB(250, 246, 240), RGB(238, 206, 164), RGB(255, 255, 255), RGB(90, 150, 232), 14 },
    { "Sesame", RGB(58, 54, 68), RGB(34, 32, 42), RGB(88, 84, 100), RGB(236, 204, 64), 12 },
    { "Pudding", RGB(242, 162, 82), RGB(202, 112, 50), RGB(255, 226, 190), RGB(112, 192, 92), 13 },
};

static Kitten kit[NK];
static int cur_x, cur_y, laser, sel, food, cup_state, cup_t, couch_marks, paused, intro;
static int exp_q16[128];
static struct { int x, y, t; char c; u16 col; } fx[12];

/* ---------------------------------------------------------------- the brain */
static i8 *brain(int k) { return save.kittens[k].brain; }

static void init_brain(int k)
{
    /* rows: eat, nap, yarn, post, couch, cup; columns: bias, hunger, tired, bored (weight / 32) */
    static const i8 base[6][NF] = {
        { -64, 64, 0, 0 }, { -64, 0, 64, 0 }, { -16, 0, 0, 24 },
        { -16, 0, 0, 16 }, { -16, 0, 0, 32 }, { -24, 0, 0, 36 },
    };
    i8 *w = brain(k);
    for (int a = 0; a < 6; a++)
        for (int f = 0; f < NF; f++)
            w[a * NF + f] = (i8)iclamp(base[a][f] + (f ? rnd_range(-10, 10) : rnd_range(-6, 6)), -127, 127);
    save.kittens[k].trust = 60;
    save.kittens[k].name_idx = (u8)k;
}

static void features(const Kitten *c, int *x)
{
    x[0] = 256;
    x[1] = c->hunger;
    x[2] = c->tired;
    x[3] = c->bored;
}

/* p[a] in Q8 (sums to about 256) */
static void policy(int k, const int *x, int *p)
{
    const i8 *w = brain(k);
    int s[NA], mx = 0;
    for (int a = 0; a < NA; a++) {
        s[a] = 0;
        if (a < 6)
            for (int f = 0; f < NF; f++)
                s[a] += w[a * NF + f] * x[f] >> 5;
        if (a == 0 || s[a] > mx) mx = s[a];
    }
    int sum = 0;
    for (int a = 0; a < NA; a++) {
        int d = (mx - s[a]) >> 3;                    /* exp((s - max) / 128) */
        s[a] = exp_q16[d > 127 ? 127 : d];
        sum += s[a];
    }
    for (int a = 0; a < NA; a++)
        p[a] = s[a] * 256 / (sum ? sum : 1);
}

static int choose(int k, const Kitten *c)
{
    int x[NF], p[NA];
    features(c, x);
    policy(k, x, p);
    int r = rnd_range(0, 255);
    for (int a = 0; a < NA; a++) {
        if (r < p[a]) return a;
        r -= p[a];
    }
    return A_IDLE;
}

/* REINFORCE with reward +1 (praise) or -1 ("no!") for the behaviour the kitten just did. */
static void learn(int k, const Kitten *c, int act, int reward)
{
    if (act == A_IDLE)
        return;
    int x[NF], p[NA];
    features(c, x);
    x[3] = imax(x[3], 160);                          /* judged as the choice a bored kitten makes */
    policy(k, x, p);
    i8 *w = brain(k);
    for (int a = 0; a < 6; a++) {
        int g = (a == act ? 256 : 0) - p[a];
        for (int f = 0; f < NF; f++) {
            int d = reward * g * x[f] / (256 * 18);
            w[a * NF + f] = (i8)iclamp(w[a * NF + f] + d, -127, 127);
        }
    }
}

/* How well the house rules have sunk in: of the things a bored kitten might do, the share that are allowed. */
static int learned_pct(int k)
{
    int x[NF] = { 256, 64, 64, 255 }, p[NA];
    policy(k, x, p);
    int good = p[A_PLAY] + p[A_POST], bad = p[A_COUCH] + p[A_KNOCK];
    return good * 100 / imax(1, good + bad);
}

/* ---------------------------------------------------------------- places in the room */
#define FLOOR_Y 150
static void act_target(int a, int *x, int *y)
{
    switch (a) {
    case A_EAT: *x = 262; *y = 156; break;
    case A_NAP: *x = 218; *y = 154; break;
    case A_POST: *x = 108; *y = 132; break;
    case A_COUCH: *x = 98; *y = 128; break;
    case A_KNOCK: *x = 272; *y = 136; break;
    default: *x = rnd_range(30, 290); *y = rnd_range(132, 168); break;
    }
}

static int yarn_x = 160, yarn_y = 160, yarn_vx;

static void spark(int x, int y, char c, u16 col)
{
    for (int i = 0; i < 12; i++)
        if (fx[i].t <= 0) {
            fx[i].x = x; fx[i].y = y; fx[i].t = 50; fx[i].c = c; fx[i].col = col;
            return;
        }
}

static void think(int k, int sit, int prio)
{
    Kitten *c = &kit[k];
    if (c->thought_t > 0 && prio < 2)
        return;
    c->thought_t = 60 * 12;
    chat_say(coats[k].name, coats[k].fur == RGB(58, 54, 68) ? C_GREY : coats[k].fur, coats[k].voice, SPK_KITTEN,
             sit, 0, 0, prio);
}

/* ---------------------------------------------------------------- behaviour */
static void start_act(int k, Kitten *c, int a)
{
    c->act = a;
    c->act_t = 0;
    if (a == A_KNOCK && cup_state != 0) a = c->act = A_PLAY;
    if (a == A_EAT && food == 0) {                   /* empty bowl: sit by it and complain */
        think(k, SIT_K_HUNGRY, 2);
    }
    if (a == A_PLAY) { c->tx = yarn_x - 8; c->ty = yarn_y; }
    else act_target(a, &c->tx, &c->ty);
    c->tx += k * 3 - 3;
    c->state = K_WALK;
}

static void finish_act(int k, Kitten *c)
{
    switch (c->act) {
    case A_EAT:
        if (food > 0) { food--; c->hunger = 0; think(k, SIT_K_HAPPY, 1); }
        break;
    case A_NAP: c->tired = 0; break;
    case A_PLAY: c->bored = imax(0, c->bored - 170); break;
    case A_POST: c->bored = imax(0, c->bored - 130); break;
    case A_COUCH: c->bored = imax(0, c->bored - 150); couch_marks = imin(couch_marks + 1, 9); break;
    case A_KNOCK: c->bored = imax(0, c->bored - 210); break;
    }
    c->last_act = c->act;
    c->last_t = 0;
    c->state = K_SIT;
    c->t = 0;
    c->act = A_IDLE;
}

static int act_len(int a)
{
    static const int len[NA] = { 170, 540, 220, 150, 150, 70, 90 };
    return len[a];
}

static void update_kitten(int k)
{
    Kitten *c = &kit[k];
    c->t++;
    c->last_t++;
    if (c->thought_t > 0) c->thought_t--;
    if (frame_count % 22 == (u32)k) c->hunger = imin(255, c->hunger + 1);
    if (frame_count % 34 == (u32)k) c->tired = imin(255, c->tired + 1);
    if (frame_count % 12 == (u32)k) c->bored = imin(255, c->bored + 1);
    /* the laser dot wins over everything except sleep */
    int dx = cur_x - c->x, dy = cur_y - c->y;
    if (laser && c->state != K_DO && iabs(dx) + iabs(dy) < 140 && c->state < K_JUMP_UP) {
        c->state = K_CHASE;
        c->act = A_IDLE;
    }
    switch (c->state) {
    case K_SIT:
        if (c->t > 70 + k * 23) {
            int a = choose(k, c);
            if (a == A_IDLE) { c->t = 0; act_target(A_IDLE, &c->tx, &c->ty); c->state = K_WALK; c->act = A_IDLE; }
            else start_act(k, c, a);
        }
        break;
    case K_WALK:
    case K_CHASE: {
        int gx = c->state == K_CHASE ? cur_x : c->tx, gy = c->state == K_CHASE ? cur_y + 4 : c->ty;
        int speed = c->state == K_CHASE ? 2 : 1;
        dx = gx - c->x;
        dy = gy - c->y;
        if (dx) c->dir = dx > 0 ? 1 : -1;
        c->x += iclamp(dx, -speed, speed);
        if (frame_count % 2 == 0 || c->state == K_CHASE) c->y += iclamp(dy, -1, 1);
        c->y = iclamp(c->y, 128, 172);
        if (c->state == K_CHASE) {
            if (!laser) { c->state = K_SIT; c->t = 0; }
            else if (iabs(dx) + iabs(dy) < 6) { c->bored = imax(0, c->bored - 3); c->bob = (int)(frame_count / 4) % 2; }
            break;
        }
        if (iabs(dx) + iabs(dy) <= 1) {
            if (c->act == A_IDLE) { c->state = K_SIT; c->t = 0; }
            else if (c->act == A_KNOCK) { c->state = K_JUMP_UP; c->t = 0; }
            else { c->state = K_DO; c->act_t = 0; }
        }
        break;
    }
    case K_DO:
        c->act_t++;
        if (c->act == A_PLAY && c->act_t % 40 == 0) { yarn_vx = c->dir * 3; sfx(SFX_POP); }
        if ((c->act == A_POST || c->act == A_COUCH) && c->act_t % 24 == 0) sfx(SFX_STEP);
        if (c->act == A_NAP && c->act_t % 50 == 0) spark(c->x + 4, c->y - 14, 'z', C_SKY);
        if (c->act_t >= act_len(c->act)) finish_act(k, c);
        break;
    case K_JUMP_UP:                                     /* up onto the table for the cup */
        c->y = 136 - imin(c->t, 20) * 32 / 20 + (c->t < 10 ? -c->t / 2 : (c->t - 20) / 4);
        if (c->t >= 20) { c->state = K_ON_TABLE; c->y = 104; c->t = 0; c->dir = 1; }
        break;
    case K_ON_TABLE:
        if (c->t == 40 && cup_state == 0) { cup_state = 1; cup_t = 0; sfx(SFX_WHOOSH); }
        if (c->t >= 70) { c->state = K_JUMP_DOWN; c->t = 0; }
        break;
    case K_JUMP_DOWN:
        c->y = 104 + c->t * 32 / 20;
        c->x += 1;
        if (c->t >= 20) { c->y = 136; finish_act(k, c); }
        break;
    case K_FLAT:
    case K_PURR:
        if (c->t > 60) { c->state = K_SIT; c->t = 0; }
        break;
    }
    /* things that come to mind */
    if (!c->thought_t && c->state == K_SIT) {
        if (c->tired > 210) think(k, SIT_K_SLEEPY, 1);
        else if (c->bored > 200) think(k, SIT_K_PLAY, 1);
        else if (rnd_range(0, 1500) == 0) think(k, SIT_K_CURIOUS, 1);
    }
    KittenSave *s = &save.kittens[k];
    s->hunger = (u8)c->hunger;
    s->energy = (u8)(255 - c->tired);
    s->fun = (u8)(255 - c->bored);
}

static int kitten_under(int x, int y)
{
    int best = -1, bd = 22;
    for (int k = 0; k < NK; k++) {
        int d = iabs(kit[k].x - x) + iabs(kit[k].y - 6 - y);
        if (d < bd) { bd = d; best = k; }
    }
    return best;
}

static void touch(int k, int good)
{
    Kitten *c = &kit[k];
    KittenSave *s = &save.kittens[k];
    int doing = c->state == K_DO || c->state == K_WALK || (c->state >= K_JUMP_UP && c->state <= K_JUMP_DOWN) ? c->act : -1;
    int judged = doing >= 0 ? doing : (c->last_t < 180 ? c->last_act : -1);
    if (good) {
        s->trust = (u8)imin(255, s->trust + 4);
        spark(c->x, c->y - 16, '\x01', C_PINK);
        sfx(SFX_PURR);
        if (judged >= 0 && judged != A_IDLE) {
            learn(k, c, judged, +1);
            spark(c->x + 6, c->y - 20, '+', C_MINT);
            think(k, SIT_K_PRAISED, 2);
        } else {
            c->bored = imax(0, c->bored - 25);
            if (rnd_range(0, 2) == 0) think(k, SIT_K_HAPPY, 1);
        }
        if (c->state == K_SIT) { c->state = K_PURR; c->t = 0; }
    } else {
        s->trust = (u8)imax(0, s->trust - 2);
        sfx(SFX_MEOW_SAD);
        if (judged >= 0 && judged != A_IDLE) {
            learn(k, c, judged, -1);
            spark(c->x + 6, c->y - 20, '-', C_RED);
        }
        think(k, SIT_K_SCOLDED, 2);
        if (c->state <= K_DO) { c->act = A_IDLE; c->state = K_FLAT; c->t = 0; }
    }
    s->learned = (u8)learned_pct(k);
    char *b = log_buf(), *q = str_cpy(b, good ? "kittens: praised " : "kittens: told no ");
    q = str_cpy(q, coats[k].name);
    q = str_cpy(q, " (");
    q = str_cpy(q, judged >= 0 ? act_names[judged] : "nothing in particular");
    q = str_cpy(q, "), rules learned ");
    str_cpy(str_int(q, s->learned), "%");
    platform_log(b);
}

/* ---------------------------------------------------------------- scene */
static void kit_enter(void)
{
    music(MUS_KITTENS);
    live_ban_he = 0;
    chat_clear();
    dlg_clear();
    exp_q16[0] = 65536;
    for (int i = 1; i < 128; i++)
        exp_q16[i] = (int)((i64)exp_q16[i - 1] * 61565 >> 16);   /* exp(-1/16) steps: exp(-d/128) at d = 8i */
    for (int k = 0; k < NK; k++) {
        int blank = 1;
        for (int f = 0; f < 24; f++) if (save.kittens[k].brain[f]) blank = 0;
        if (blank) init_brain(k);
        save.kittens[k].learned = (u8)learned_pct(k);
        KittenSave *s = &save.kittens[k];
        kit[k] = (Kitten){ 60 + k * 90, 140 + k * 8, 0, 0, 1, K_SIT, k * 30, A_IDLE, 0, A_IDLE, 999, 0, 0, 60 * (3 + k * 4),
                           s->hunger, 255 - s->energy, 255 - s->fun };
        if (s->energy == 0 && s->fun == 0 && s->hunger == 0)
            kit[k].hunger = 120, kit[k].tired = 60, kit[k].bored = 150 + k * 30;
    }
    cur_x = 160; cur_y = 120; laser = 0; sel = 0;
    food = 3; cup_state = 0; couch_marks = 2; paused = 0;
    for (int i = 0; i < 12; i++) fx[i].t = 0;
    save.kitten_days++;
    dlg_push(CH_NIA, EX_NEUTRAL, "Mochi, Sesame and Pudding. Fostering them until they find homes.");
    dlg_push(CH_NIA, EX_SMUG, "Each one has a tiny neural net for a brain. It learns from you, and it keeps what it learns.");
    dlg_push(CH_NIA, EX_NEUTRAL, "^3A^0 praises or pets. ^3B^0 says no. ^3X^0 fills the bowl. Hold ^3Y^0 for the laser dot.");
    dlg_push(CH_NIA, EX_TIRED, "Teach them the scratching post. The couch has suffered enough.");
    intro = 1;
}

static void kit_update(void)
{
    if (intro) {
        dlg_update();
        if (dlg_just_finished()) intro = 0;
        return;
    }
    if (paused) {
        if (any_pressed(BTN_A | BTN_START)) { paused = 0; sfx(SFX_OK); }
        else if (any_pressed(BTN_B)) { sfx(SFX_BACK); scene_fade_to(&scene_menu); }
        return;
    }
    if (any_pressed(BTN_START)) { paused = 1; sfx(SFX_POP); return; }
    chat_update();
    int sp = btn_held(0, BTN_DIRS) && frame_count % 2 ? 3 : 2;
    {
        if (btn_held(0, BTN_LEFT) || btn_held(1, BTN_LEFT)) cur_x -= sp;
        if (btn_held(0, BTN_RIGHT) || btn_held(1, BTN_RIGHT)) cur_x += sp;
        if (btn_held(0, BTN_UP) || btn_held(1, BTN_UP)) cur_y -= sp;
        if (btn_held(0, BTN_DOWN) || btn_held(1, BTN_DOWN)) cur_y += sp;
    }
    cur_x = iclamp(cur_x, 8, SCREEN_W - 8);
    cur_y = iclamp(cur_y, 30, SCREEN_H - 6);
    laser = btn_held(0, BTN_Y) || btn_held(1, BTN_Y);
    int k = kitten_under(cur_x, cur_y);
    if (k >= 0) sel = k;
    if (any_pressed(BTN_A) && k >= 0) touch(k, 1);
    if (any_pressed(BTN_B) && k >= 0) touch(k, 0);
    if (any_pressed(BTN_X)) {
        food = 3;
        sfx(SFX_COIN);
        spark(262, 140, '\x04', C_GOLD);
    }
    for (int i = 0; i < NK; i++)
        update_kitten(i);
    /* the yarn rolls and slows down; the cup falls and Nia puts it back */
    yarn_x = iclamp(yarn_x + yarn_vx, 140, 240);
    if (frame_count % 8 == 0 && yarn_vx) yarn_vx -= yarn_vx > 0 ? 1 : -1;
    if (cup_state == 1 && ++cup_t > 18) { cup_state = 2; cup_t = 0; sfx(SFX_HIT); }
    if (cup_state == 2 && ++cup_t > 60 * 6) { cup_state = 0; }
    for (int i = 0; i < 12; i++) if (fx[i].t > 0) { fx[i].t--; if (fx[i].t % 3 == 0) fx[i].y--; }
}

/* ---------------------------------------------------------------- drawing */
static void draw_kitten(int k, const Kitten *c)
{
    const u16 ink = RGB(46, 34, 52);
    u16 fur = coats[k].fur, dark = coats[k].dark;
    int x = c->x, y = c->y, d = c->dir;
    int pose = c->state;
    ellipse_fill(x, y + 1, 9, 2, RGB(150, 110, 90));         /* shadow */
    if (pose == K_DO && c->act == A_NAP) {                   /* curled up asleep */
        ellipse_fill(x, y - 4, 9, 6, ink);
        ellipse_fill(x, y - 4, 8, 5, fur);
        circle_fill(x + 5 * d, y - 4, 4, fur);
        hline(x + 3 * d - 1, y - 4, 3, ink);
        tri_fill(x + 3 * d, y - 8, x + 6 * d, y - 12, x + 8 * d, y - 7, fur);
        rect(x - 7, y - 1, 14, 2, dark);
        return;
    }
    int standing = pose == K_DO && (c->act == A_POST || c->act == A_COUCH);
    int flat = pose == K_FLAT;
    int walk = (pose == K_WALK || pose == K_CHASE) ? (int)(frame_count / 6) % 2 : 0;
    /* tail */
    int tx = x - 7 * d, ty = y - 6;
    for (int i = 0; i < 6; i++) {
        int px = tx - d * (i / 2), py = ty - i * 2 + (standing ? 4 : 0) + (flat ? i : 0);
        rect(px - 1, py - 1, 3, 3, ink);
        pset(px, py, i > 3 ? dark : fur);
        pset(px, py + 1, fur);
    }
    if (standing) {                                          /* up on the hind legs, scratching */
        ellipse_fill(x, y - 9, 5, 8, ink);
        ellipse_fill(x, y - 9, 4, 7, fur);
        rect(x + 3 * d, y - 16 + ((int)(frame_count / 6) % 2) * 3, 4, 2, fur);
        circle_fill(x + 1 * d, y - 18, 5, ink);
        circle_fill(x + 1 * d, y - 18, 4, fur);
        tri_fill(x - 3 + d, y - 20, x - 2 + d, y - 26, x + d, y - 21, fur);
        tri_fill(x + 3 + d, y - 20, x + 2 + d, y - 26, x + d, y - 21, fur);
        pset(x + 2 * d, y - 18, coats[k].eye);
        return;
    }
    int body_y = y - 5 + (flat ? 2 : 0) + (pose == K_CHASE ? c->bob : 0);
    /* legs */
    for (int l = 0; l < 4; l++) {
        int lx = x - 5 + l * 3 + (l >= 2 ? 1 : 0), up = walk && (l % 2 == 0) ? 1 : 0;
        rect(lx, body_y + 2 - up, 2, 4 - (flat ? 2 : 0), ink);
        pset(lx, body_y + 4 - up, fur);
    }
    ellipse_fill(x, body_y, 8, 5, ink);
    ellipse_fill(x, body_y, 7, 4, fur);
    ellipse_fill(x + 1 * d, body_y + 2, 4, 2, coats[k].belly);
    if (k == 2) for (int s = -4; s <= 2; s += 3) vline(x + s, body_y - 3, 3, dark);   /* tabby stripes */
    if (k == 0) circle_fill(x - 3 * d, body_y - 1, 2, dark);                          /* cream patch */
    /* head */
    int hx = x + 7 * d, hy = body_y - 4 + (flat ? 3 : 0) + (pose == K_DO && c->act == A_EAT ? 5 : 0);
    circle_fill(hx, hy, 5, ink);
    circle_fill(hx, hy, 4, fur);
    int ear = flat ? 2 : 0;
    tri_fill(hx - 4, hy - 2 + ear, hx - 3, hy - 8 + ear * 2, hx, hy - 3, ink);
    tri_fill(hx + 4, hy - 2 + ear, hx + 3, hy - 8 + ear * 2, hx, hy - 3, ink);
    tri_fill(hx - 3, hy - 2 + ear, hx - 3, hy - 6 + ear * 2, hx - 1, hy - 3, fur);
    tri_fill(hx + 3, hy - 2 + ear, hx + 3, hy - 6 + ear * 2, hx + 1, hy - 3, fur);
    pset(hx - 3, hy - 4 + ear * 2, RGB(255, 170, 190));
    pset(hx + 3, hy - 4 + ear * 2, RGB(255, 170, 190));
    if (pose == K_PURR || flat) {
        hline(hx - 3, hy, 2, ink);
        hline(hx + 1, hy, 2, ink);
    } else {
        pset(hx - 2, hy, coats[k].eye); pset(hx - 2, hy - 1, ink);
        pset(hx + 2, hy, coats[k].eye); pset(hx + 2, hy - 1, ink);
    }
    pset(hx + d, hy + 2, RGB(255, 150, 170));
    if (c->act == A_EAT && pose == K_DO && food == 0) text(hx - 1, hy - 16, "?", C_WHITE);
}

static void draw_room(void)
{
    gradient_v(0, 24, SCREEN_W, 82, RGB(232, 214, 240), RGB(214, 190, 226));
    for (int x = 6; x < SCREEN_W; x += 24)                   /* wallpaper paws */
        for (int y = 32 + (x / 24 % 2) * 12; y < 100; y += 24)
            text(x, y, "\x04", RGB(222, 198, 232));
    /* window */
    gradient_v(142, 32, 56, 44, RGB(150, 206, 255), RGB(255, 236, 214));
    rect_line(141, 31, 58, 46, C_WHITE);
    vline(170, 32, 44, C_WHITE);
    rect(138, 76, 64, 3, C_WHITE);
    /* floor */
    rect(0, 104, SCREEN_W, 76, RGB(204, 150, 110));
    for (int r = 0, y = 104; y < SCREEN_H; r++, y += 9) {
        hline(0, y, SCREEN_W, RGB(170, 118, 86));
        for (int x = (r * 41) % 70; x < SCREEN_W; x += 70) vline(x, y, 9, RGB(178, 126, 92));
    }
    rect(0, 100, SCREEN_W, 4, RGB(236, 220, 236));
    /* couch, with the scratch marks it has collected */
    round_box(6, 82, 86, 26, RGB(120, 70, 110), RGB(176, 110, 160));
    rect(4, 104, 90, 18, RGB(196, 128, 178));
    round_box(2, 96, 10, 28, RGB(120, 70, 110), RGB(176, 110, 160));
    round_box(86, 96, 10, 28, RGB(120, 70, 110), RGB(176, 110, 160));
    for (int m = 0; m < couch_marks; m++)
        line(88 + (m % 3) * 2, 104 + (m / 3) * 6, 90 + (m % 3) * 2, 108 + (m / 3) * 6, RGB(250, 230, 240));
    rect(8, 122, 3, 5, RGB(90, 60, 50));
    rect(84, 122, 3, 5, RGB(90, 60, 50));
    /* scratching post */
    rect(112, 126, 22, 5, RGB(150, 110, 80));
    rect(118, 88, 10, 38, RGB(222, 196, 150));
    for (int y = 90; y < 126; y += 3) hline(118, y, 10, RGB(196, 166, 120));
    round_box(110, 84, 26, 6, RGB(120, 84, 60), RGB(170, 214, 200));
    /* bed, bowl, yarn */
    ellipse_fill(218, 152, 20, 7, RGB(140, 90, 130));
    ellipse_fill(218, 150, 17, 5, RGB(250, 214, 150));
    ellipse_fill(266, 154, 9, 3, RGB(90, 160, 220));
    if (food) ellipse_fill(266, 152, 6, 2, RGB(170, 110, 70));
    for (int i = 0; i < food; i++) pset(262 + i * 4, 151, RGB(120, 70, 40));
    circle_fill(yarn_x, yarn_y - 3, 5, RGB(234, 90, 120));
    line(yarn_x - 4, yarn_y - 5, yarn_x + 3, yarn_y - 1, RGB(255, 160, 180));
    line(yarn_x + 5, yarn_y - 2, yarn_x + 14, yarn_y + 1, RGB(234, 90, 120));
    /* table with the cup */
    rect(236, 104, 70, 5, RGB(160, 110, 80));
    rect(240, 109, 3, 26, RGB(130, 88, 64));
    rect(300, 109, 3, 26, RGB(130, 88, 64));
    if (cup_state == 0) {
        round_box(288, 96, 8, 8, RGB(90, 70, 90), C_WHITE);
        pset(296, 99, RGB(90, 70, 90));
    } else if (cup_state == 1) {
        int t = cup_t;
        round_box(290 + t / 2, 96 + t * t / 8, 8, 8, RGB(90, 70, 90), C_WHITE);
    } else {
        rect(292, 150, 3, 2, C_WHITE); rect(297, 152, 4, 2, C_WHITE); rect(301, 149, 2, 2, C_WHITE);
    }
}

static void draw_brain(int k)
{
    int x0 = 214, y0 = 28;
    round_box(x0, y0, 102, 70, C_GOLD, RGB(36, 30, 58));
    text(x0 + 5, y0 + 3, coats[k].name, C_GOLD);
    char buf[16];
    str_cpy(str_int(buf, save.kittens[k].learned), "% rules");
    text(x0 + 54, y0 + 3, buf, C_MINT);
    int x[NF], p[NA];
    features(&kit[k], x);
    policy(k, x, p);
    for (int a = 0; a < NA; a++) {
        int y = y0 + 13 + a * 8;
        text(x0 + 5, y, act_names[a], act_naughty[a] ? C_RED : C_GREY);
        rect(x0 + 34, y + 1, 62, 5, C_PANEL);
        rect(x0 + 34, y + 1, imax(1, p[a] * 62 / 256), 5, act_naughty[a] ? C_RED : a == A_POST || a == A_PLAY ? C_MINT : C_SKY);
    }
}

static void kit_draw(void)
{
    draw_room();
    /* draw back to front */
    int order[NK] = { 0, 1, 2 };
    for (int i = 1; i < NK; i++)
        for (int j = i; j > 0 && kit[order[j]].y < kit[order[j - 1]].y; j--) {
            int t = order[j]; order[j] = order[j - 1]; order[j - 1] = t;
        }
    for (int i = 0; i < NK; i++)
        draw_kitten(order[i], &kit[order[i]]);
    for (int i = 0; i < 12; i++)
        if (fx[i].t > 0) {
            char s[2] = { fx[i].c, 0 };
            text(fx[i].x, fx[i].y, s, fx[i].col);
        }
    /* the hand, or the laser dot */
    if (laser) {
        circle_fill(cur_x, cur_y, 3, blend(RGB(204, 150, 110), C_RED, 120));
        circle_fill(cur_x, cur_y, 1, RGB(255, 60, 60));
    } else {
        circle_fill(cur_x, cur_y + 2, 5, RGB(58, 40, 62));
        circle_fill(cur_x, cur_y + 2, 4, RGB(255, 226, 214));
        for (int t = -1; t <= 1; t++) circle_fill(cur_x + t * 4, cur_y - 4 + iabs(t), 2, RGB(255, 190, 200));
    }
    int k = kitten_under(cur_x, cur_y);
    if (k >= 0 && !laser)
        text(cur_x + 8, cur_y - 12, "^3A^0 \x01  ^3B^0 no", C_WHITE);
    /* thoughts, the brain of whoever you last pointed at, and help */
    rect(0, 0, SCREEN_W, 24, C_INK);
    chat_draw(4, 2, 312);
    draw_brain(sel);
    rect_blend(0, 170, 206, 10, C_INK, 150);
    text(4, 171, "^3X^0 food  ^3Y^0 laser  ^3START^0 menu", C_GREY);
    if (intro) {
        rect_blend(0, 24, SCREEN_W, SCREEN_H - 24, C_NIGHT, 110);
        draw_bust(CH_NIA, dlg_expr(), 248, 58, 1);
        dlg_draw(4, 124, 312, 52);
    }
    if (paused) {
        rect_blend(0, 0, SCREEN_W, SCREEN_H, C_NIGHT, 160);
        round_box(70, 60, 180, 56, C_GOLD, C_PANEL);
        text_center(SCREEN_W / 2, 68, "Kittens", C_GOLD, C_INK);
        for (int i = 0; i < NK; i++) {
            char b[40], *p = b;
            p = str_cpy(p, coats[i].name);
            p = str_cpy(p, ": ");
            p = str_int(p, save.kittens[i].learned);
            str_cpy(p, "% of the house rules");
            text(82, 80 + i * 9, b, C_WHITE);
        }
        text_center(SCREEN_W / 2, 108, "^3A^0 back   ^3B^0 to Nia's desk", C_GREY, C_INK);
    }
}

const Scene scene_kittens = { kit_enter, kit_update, kit_draw, "kittens" };
