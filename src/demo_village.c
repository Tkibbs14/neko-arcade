/* Build 5 - Nekomura: a small village that remembers you. Six villagers (Nia, Mako, Shio and Mocha, plus
 * Kuro the old fisherman and Suzu the florist) gossip about what you actually did in the other builds,
 * straight from the save: Mako's hockey record, Shio's cases, shifts at the cafe, Nia's kittens, the fish
 * you caught here. Every line is written live by NekoLM. Gather daisies, berries and acorns, fish at the
 * pond, and learn who loves which gift; friendship and gifts are kept in the save. */
#include "engine.h"
#include "save.h"
#include "cast.h"
#include "chibi.h"
#include "dialog.h"
#include "chatter.h"
#include "live.h"
#include "nekolm.h"

#define NV 6
#define DAY_LEN (60 * 60 * 4)
enum { IT_DAISY, IT_FISH, IT_BERRIES, IT_ACORN, NITEMS };
static const char *const item_names[NITEMS] = { "daisy", "fish", "basket of berries", "shiny acorn" };

static const struct {
    const char *name, *pron;
    int look, spk, cast_id, voice;
    u16 color;
    int hx, hy;
    u8 love, ok;
} vills[NV] = {
    { "Nia", "her", LK_NIA, SPK_NIA, CH_NIA, 0, RGB(156, 112, 226), 108, 112, IT_FISH, IT_ACORN },
    { "Mako", "her", LK_MAKO, SPK_MAKO, CH_MAKO, 0, RGB(224, 74, 162), 196, 108, IT_BERRIES, IT_FISH },
    { "Shio", "her", LK_SHIO, SPK_SHIO, CH_SHIO, 0, RGB(146, 222, 200), 58, 126, IT_ACORN, IT_DAISY },
    { "Mocha", "her", LK_MOCHA, SPK_MOCHA, CH_MOCHA, 0, RGB(200, 142, 92), 146, 62, IT_BERRIES, IT_DAISY },
    { "Kuro", "him", LK_KURO, SPK_KURO, -1, -10, RGB(232, 200, 84), 240, 124, IT_DAISY, IT_BERRIES },
    { "Suzu", "her", LK_SUZU, SPK_SUZU, -1, 9, RGB(232, 142, 62), 252, 62, IT_DAISY, IT_ACORN },
};

typedef struct { int x, y, tx, ty, dir, walk, wait, talked, last_topic; } Villager;
static Villager vil[NV];

enum { M_WALK, M_TALK, M_MENU, M_GIFT, M_FISH, M_SLEEP };
static int mode, mode_t, talk_v, menu_sel, gift_sel;
static int px, py, pdir = 1, pwalk, day_t, inv[NITEMS];
static int daisies, berries, acorns;                    /* bitmask of daisy spots / counts left today */
static int fish_state, fish_t, fish_wait, ambient_t;
static char fish_n[8];

static const i16 daisy_spots[6][2] = { { 22, 150 }, { 40, 162 }, { 64, 156 }, { 86, 166 }, { 18, 168 }, { 104, 158 } };

/* ---------------------------------------------------------------- the map */
typedef struct { i16 x, y, w, h; } Box;
static const Box solids[] = {
    { 12, 20, 60, 36 },      /* your house */
    { 118, 16, 56, 30 },     /* Mocha's stall */
    { 224, 16, 60, 34 },     /* Suzu's flowers */
    { 150, 82, 30, 22 },     /* fountain */
    { 16, 100, 24, 18 },     /* oak tree */
    { 128, 150, 20, 12 },    /* berry bush */
    { 300, 60, 16, 20 }, { 88, 70, 14, 12 }, { 194, 140, 14, 12 },   /* trees */
};
#define POND_X 262
#define POND_Y 150
#define POND_RX 46
#define POND_RY 22

static int in_pond(int x, int y, int grow)
{
    int dx = x - POND_X, dy = y - POND_Y, rx = POND_RX + grow, ry = POND_RY + grow;
    return (i64)dx * dx * ry * ry + (i64)dy * dy * rx * rx < (i64)rx * rx * ry * ry;
}

static int on_pier(int x, int y) { return x >= 232 && x <= 246 && y >= 126 && y <= 146; }

static int blocked(int x, int y)
{
    if (x < 6 || x > SCREEN_W - 6 || y < 24 || y > SCREEN_H - 3)
        return 1;
    for (unsigned i = 0; i < sizeof solids / sizeof solids[0]; i++) {
        const Box *b = &solids[i];
        if (x + 4 > b->x && x - 4 < b->x + b->w && y > b->y && y - 4 < b->y + b->h)
            return 1;
    }
    return in_pond(x, y, 0) && !on_pier(x, y);
}

static int night_level(void)                               /* 0 day .. 160 night */
{
    int t = day_t * 4 / DAY_LEN;                           /* quarters of the day */
    if (t < 3) return 0;
    if (day_t >= DAY_LEN) return 160;
    return (day_t - DAY_LEN * 3 / 4) * 160 / (DAY_LEN / 4);
}

static void draw_item_icon(int it, int x, int y)
{
    switch (it) {
    case IT_DAISY:
        for (int a = 0; a < 5; a++) circle_fill(x + 4 + icos(a * 51) * 3 / 256, y + 4 + isin(a * 51) * 3 / 256, 1, C_WHITE);
        pset(x + 4, y + 4, C_GOLD);
        break;
    case IT_FISH:
        ellipse_fill(x + 3, y + 4, 3, 2, RGB(90, 150, 230));
        tri_fill(x + 6, y + 4, x + 8, y + 2, x + 8, y + 6, RGB(90, 150, 230));
        pset(x + 2, y + 3, C_INK);
        break;
    case IT_BERRIES:
        rect(x + 1, y + 4, 7, 4, RGB(170, 120, 70));
        circle_fill(x + 3, y + 3, 1, C_RED); circle_fill(x + 6, y + 3, 1, C_RED); pset(x + 4, y + 2, C_RED);
        break;
    case IT_ACORN:
        ellipse_fill(x + 4, y + 5, 2, 3, RGB(200, 140, 70));
        rect(x + 1, y + 2, 7, 2, RGB(120, 84, 50));
        pset(x + 4, y + 1, RGB(120, 84, 50));
        break;
    }
}

static void draw_tree(int x, int y, int big)
{
    rect(x - 2, y - 4, 5, 10, RGB(120, 84, 58));
    circle_fill(x, y - 10 - big * 2, 9 + big * 3, RGB(52, 120, 72));
    circle_fill(x - 3, y - 13 - big * 2, 6 + big * 2, RGB(80, 160, 90));
    circle_fill(x - 4, y - 15 - big * 2, 2, RGB(140, 210, 120));
}

static void draw_building(int x, int y, int w, int h, u16 wall, u16 roof, u16 awning)
{
    tri_fill(x - 4, y + 12, x + w + 4, y + 12, x + w / 2, y - 6, roof);
    rect(x, y + 12, w, h - 12, wall);
    rect_line(x, y + 12, w, h - 12, blend(wall, C_INK, 80));
    if (awning)
        for (int i = 0; i < w; i += 8)
            rect(x + i, y + 14, 4, 5, awning);
}

static void draw_map(void)
{
    gradient_v(0, 12, SCREEN_W, 168, RGB(150, 214, 120), RGB(118, 188, 100));
    for (int i = 0; i < 90; i++) {                          /* grass tufts */
        u32 h = (u32)i * 2654435761u;
        pset((int)(h % SCREEN_W), 14 + (int)((h >> 11) % 164), RGB(96, 166, 86));
    }
    /* paths: from each door to the square */
    u16 path = RGB(226, 204, 158);
    rect(38, 56, 12, 44, path);
    rect(38, 92, 240, 12, path);
    rect(140, 46, 12, 50, path);
    rect(246, 50, 12, 46, path);
    rect(160, 92, 12, 70, path);
    circle_fill(165, 98, 26, path);
    /* your house, Mocha's stall "Hitoiki", Suzu's flower shop */
    draw_building(12, 16, 60, 40, RGB(250, 236, 212), RGB(190, 90, 80), 0);
    rect(38, 42, 10, 14, RGB(140, 96, 70));
    rect(18, 36, 10, 8, night_level() > 60 ? C_GOLD : RGB(170, 214, 255));
    draw_building(118, 12, 56, 34, RGB(236, 206, 170), RGB(150, 100, 70), C_ORANGE);
    text(126, 34, "Hitoiki", RGB(110, 70, 50));
    draw_building(224, 12, 60, 38, RGB(252, 236, 240), RGB(214, 110, 150), C_PINK);
    text(236, 36, "Suzu's", RGB(170, 70, 110));
    for (int i = 0; i < 6; i++)
        circle_fill(228 + i * 10, 52, 3, (i % 3) == 0 ? C_PINK : (i % 3) == 1 ? C_GOLD : C_SKY);
    /* the fountain */
    circle_fill(165, 93, 15, RGB(170, 170, 184));
    circle_fill(165, 93, 12, RGB(110, 180, 236));
    for (int i = 0; i < 3; i++) {
        int r = (int)((frame_count / 4 + (u32)i * 5) % 12);
        circle_line(165, 93, r, blend(RGB(110, 180, 236), C_WHITE, 150 - r * 10));
    }
    rect(163, 80, 4, 10, RGB(170, 170, 184));
    /* the pond, lily pads, Kuro's pier */
    ellipse_fill(POND_X, POND_Y, POND_RX + 2, POND_RY + 2, RGB(96, 150, 90));
    ellipse_fill(POND_X, POND_Y, POND_RX, POND_RY, RGB(84, 150, 220));
    ellipse_fill(POND_X + 6, POND_Y + 4, POND_RX - 14, POND_RY - 8, RGB(96, 166, 232));
    circle_fill(290, 146, 3, RGB(90, 170, 90));
    circle_fill(280, 162, 2, RGB(90, 170, 90));
    rect(232, 126, 14, 22, RGB(170, 120, 80));
    for (int y = 128; y < 148; y += 4) hline(232, y, 14, RGB(140, 96, 64));
    /* the meadow's daisies, the berry bush, the oak */
    for (int i = 0; i < 6; i++)
        if (daisies & (1 << i)) draw_item_icon(IT_DAISY, daisy_spots[i][0] - 4, daisy_spots[i][1] - 4);
    circle_fill(138, 154, 10, RGB(52, 124, 72));
    circle_fill(134, 151, 6, RGB(76, 150, 86));
    for (int i = 0; i < berries * 3; i++) pset(132 + (i * 7) % 14, 150 + (i * 5) % 8, C_RED);
    draw_tree(28, 116, 1);
    for (int i = 0; i < acorns; i++) draw_item_icon(IT_ACORN, 14 + i * 16, 112);
    draw_tree(308, 78, 0);
    draw_tree(95, 80, 0);
    draw_tree(201, 150, 0);
    /* benches and lamps */
    rect(96, 118, 22, 3, RGB(150, 104, 72));
    rect(98, 121, 2, 4, RGB(110, 76, 54)); rect(114, 121, 2, 4, RGB(110, 76, 54));
    for (int i = 0; i < 2; i++) {
        int lx = i ? 216 : 118, ly = 88;
        vline(lx, ly - 14, 16, C_INK);
        circle_fill(lx, ly - 15, 2, night_level() > 40 ? C_GOLD : C_GREY);
    }
}

/* ---------------------------------------------------------------- talking */
static void say_live(int v, int sit, const char *key, const char *value)
{
    if (vills[v].cast_id >= 0)
        dlg_push_live(vills[v].cast_id, EX_HAPPY, vills[v].spk, sit, key, value);
    else
        dlg_push_live_named(vills[v].name, vills[v].color, vills[v].voice, vills[v].spk, sit, key, value);
}

static int hearts(int v) { return save.hearts[v]; }

static void add_hearts(int v, int n) { save.hearts[v] = (u8)imin(100, save.hearts[v] + n); }

static void start_talk(int v)
{
    {
        char *b = log_buf(), *q = str_cpy(b, "village: talk to ");
        q = str_cpy(q, vills[v].name);
        q = str_cpy(q, ", hearts ");
        str_int(q, save.hearts[v]);
        platform_log(b);
    }
    talk_v = v;
    Villager *w = &vil[v];
    w->dir = px < w->x ? -1 : 1;
    pdir = -w->dir;
    dlg_clear();
    int sit = night_level() > 100 ? SIT_V_NIGHT : hearts(v) >= 30 ? SIT_V_HELLO_FRIEND : SIT_V_HELLO;
    say_live(v, sit, 0, 0);
    if (save.gifts_given[v] && rnd_range(0, 2) == 0) {       /* they remember your last gift */
        char *b = dlg_buf();
        char *p = str_cpy(b, vills[v].name);
        p = str_cpy(p, " still keeps the ");
        p = str_cpy(p, item_names[save.last_gift_item[v] % NITEMS]);
        p = str_cpy(p, " you gave ");
        str_cpy(str_cpy(p, vills[v].pron), ".");
        dlg_push(DLG_NARRATOR, 0, b);
    }
    mode = M_TALK;
    mode_t = 0;
}

static int pick_topic(int v)
{
    int t[9], n = 0;
    if (save.rival_wins) t[n++] = SIT_V_GOSSIP_RIVAL_WIN;
    if (save.rival_losses) t[n++] = SIT_V_GOSSIP_RIVAL_LOSE;
    if (save.cases_solved) t[n++] = SIT_V_GOSSIP_CASE;
    if (save.cafe_runs) t[n++] = SIT_V_GOSSIP_CAFE;
    if (save.kitten_days) t[n++] = SIT_V_GOSSIP_KITTENS;
    if (save.fish_caught) t[n++] = SIT_V_GOSSIP_FISH;
    t[n++] = SIT_V_SMALLTALK;
    t[n++] = SIT_V_SMALLTALK;
    int pick = t[rnd_range(0, n - 1)];
    if (pick == vil[v].last_topic && n > 2)
        pick = t[rnd_range(0, n - 1)];
    vil[v].last_topic = pick;
    return pick;
}

static void chat_with(int v)
{
    int topic = pick_topic(v);
    {
        static const struct { int sit; const char *name; } names[] = {
            { SIT_V_GOSSIP_RIVAL_WIN, "your wins vs Mako" }, { SIT_V_GOSSIP_RIVAL_LOSE, "Mako's wins" },
            { SIT_V_GOSSIP_CASE, "Shio's cases" }, { SIT_V_GOSSIP_CAFE, "the cafe" },
            { SIT_V_GOSSIP_KITTENS, "the kittens" }, { SIT_V_GOSSIP_FISH, "your fish" }, { SIT_V_SMALLTALK, "small talk" },
        };
        char *b = log_buf(), *q = str_cpy(b, "village: ");
        q = str_cpy(q, vills[v].name);
        q = str_cpy(q, " chats about ");
        for (unsigned k = 0; k < sizeof names / sizeof names[0]; k++)
            if (names[k].sit == topic) q = str_cpy(q, names[k].name);
        platform_log(b);
    }
    str_int(fish_n, save.fish_caught);
    say_live(v, topic, topic == SIT_V_GOSSIP_FISH ? "n" : 0, topic == SIT_V_GOSSIP_FISH ? fish_n : 0);
    if (!vil[v].talked) {
        vil[v].talked = 1;
        add_hearts(v, 3);
    }
    mode = M_TALK;
}

static void give(int v, int it)
{
    inv[it]--;
    int sit = it == vills[v].love ? SIT_V_GIFT_LOVE : it == vills[v].ok ? SIT_V_GIFT_OK : SIT_V_GIFT_MEH;
    say_live(v, sit, "item", item_names[it]);
    int repeat = save.gifts_given[v] && save.last_gift_item[v] == it;   /* the same gift again counts less */
    add_hearts(v, repeat ? 1 : sit == SIT_V_GIFT_LOVE ? 10 : sit == SIT_V_GIFT_OK ? 5 : 1);
    save.gifts_given[v] = (u8)imin(255, save.gifts_given[v] + 1);
    save.last_gift_item[v] = (u8)it;
    sfx(sit == SIT_V_GIFT_LOVE ? SFX_GOAL : SFX_OK);
    mode = M_TALK;
    char *b = log_buf(), *q = str_cpy(b, "village: gave ");
    q = str_cpy(q, item_names[it]);
    q = str_cpy(q, " to ");
    q = str_cpy(q, vills[v].name);
    q = str_cpy(q, sit == SIT_V_GIFT_LOVE ? " (loved it)" : sit == SIT_V_GIFT_OK ? " (fine)" : " (meh)");
    q = str_cpy(q, ", hearts ");
    str_int(q, save.hearts[v]);
    platform_log(b);
}

/* ---------------------------------------------------------------- the day */
static void new_day(void)
{
    day_t = 0;
    daisies = 0x3F;
    berries = 2;
    acorns = 2;
    for (int v = 0; v < NV; v++) vil[v].talked = 0;
    save.village_days++;
}

static void vil_enter(void)
{
    music(MUS_VILLAGE);
    live_ban_he = 0;
    chat_clear();
    dlg_clear();
    for (int v = 0; v < NV; v++)
        vil[v] = (Villager){ vills[v].hx, vills[v].hy, vills[v].hx, vills[v].hy, 1, 0, rnd_range(30, 200), 0, -1 };
    px = 43; py = 70; pdir = 1;
    for (int i = 0; i < NITEMS; i++) inv[i] = 0;
    new_day();
    fish_state = 0;
    ambient_t = 60 * 8;
    talk_v = -1;
    mode = M_TALK;
    dlg_push(DLG_NARRATOR, 0, "Nekomura. ^3A^0 talks, picks things up and fishes at the pond. Villagers remember what "
                              "you do here and in Nia's other builds.");
    if (save.village_days > 1)
        dlg_push(DLG_NARRATOR, 0, "Your house is by the path. ^3A^0 at the door sleeps until the next day.");
}

static int held(u16 mask) { return btn_held(0, mask) || btn_held(1, mask); }

static int near_villager(void)
{
    int best = -1, bd = 30;
    for (int v = 0; v < NV; v++) {
        int d = iabs(vil[v].x - px) + iabs(vil[v].y - py);
        if (d < bd) { bd = d; best = v; }
    }
    return best;
}

static void try_action(void)
{
    int v = near_villager();
    if (v >= 0) { start_talk(v); return; }
    for (int i = 0; i < 6; i++)
        if ((daisies & (1 << i)) && iabs(daisy_spots[i][0] - px) < 10 && iabs(daisy_spots[i][1] - py) < 10) {
            daisies &= ~(1 << i);
            inv[IT_DAISY] = imin(9, inv[IT_DAISY] + 1);
            sfx(SFX_COIN);
            return;
        }
    if (berries && iabs(px - 138) < 18 && iabs(py - 160) < 14) {
        berries--;
        inv[IT_BERRIES] = imin(9, inv[IT_BERRIES] + 1);
        sfx(SFX_COIN);
        return;
    }
    if (acorns && iabs(px - 28) < 22 && iabs(py - 124) < 14) {
        acorns--;
        inv[IT_ACORN] = imin(9, inv[IT_ACORN] + 1);
        sfx(SFX_COIN);
        return;
    }
    if (in_pond(px + pdir * 12, py, 0) || in_pond(px, py + 10, 0)) {
        mode = M_FISH;
        fish_state = 1;
        fish_t = 0;
        fish_wait = rnd_range(60, 260);
        sfx(SFX_SPLASH);
        return;
    }
    if (iabs(px - 43) < 12 && py < 66) {
        mode = M_SLEEP;
        menu_sel = 0;
        return;
    }
}

static void update_villagers(void)
{
    for (int v = 0; v < NV; v++) {
        Villager *w = &vil[v];
        if ((mode != M_WALK && v == talk_v) || v == 4)        /* Kuro keeps to his pier */
            continue;
        if (w->x == w->tx && w->y == w->ty) {
            if (--w->wait <= 0) {
                int nx = vills[v].hx + rnd_range(-18, 18), ny = vills[v].hy + rnd_range(-10, 10);
                if (!blocked(nx, ny)) { w->tx = nx; w->ty = ny; }
                w->wait = rnd_range(90, 300);
            }
            continue;
        }
        if (frame_count % 2) continue;
        int dx = w->tx - w->x, dy = w->ty - w->y;
        if (dx) w->dir = dx > 0 ? 1 : -1;
        int nx = w->x + iclamp(dx, -1, 1), ny = w->y + iclamp(dy, -1, 1);
        if (blocked(nx, ny) || (iabs(nx - px) < 8 && iabs(ny - py) < 6)) { w->tx = w->x; w->ty = w->y; continue; }
        w->x = nx; w->y = ny; w->walk++;
    }
}

static void vil_update(void)
{
    mode_t++;
    if (day_t < DAY_LEN) day_t++;
    dlg_update();
    chat_update();
    update_villagers();
    switch (mode) {
    case M_TALK:                                              /* after a conversation, what next? */
        if (!dlg_busy()) {
            if (talk_v >= 0) { mode = M_MENU; menu_sel = 0; }
            else mode = M_WALK;
        }
        break;
    case M_MENU: {
        int n = inv[0] + inv[1] + inv[2] + inv[3] ? 3 : 2;
        if (any_repeat(BTN_DOWN)) { menu_sel = (menu_sel + 1) % n; sfx(SFX_MOVE); }
        if (any_repeat(BTN_UP)) { menu_sel = (menu_sel + n - 1) % n; sfx(SFX_MOVE); }
        if (any_pressed(BTN_B)) { mode = M_WALK; talk_v = -1; sfx(SFX_BACK); break; }
        if (any_pressed(BTN_A)) {
            int opt = n == 3 ? menu_sel : (menu_sel == 0 ? 0 : 2);
            sfx(SFX_OK);
            if (opt == 0) chat_with(talk_v);
            else if (opt == 1) { mode = M_GIFT; gift_sel = 0; while (!inv[gift_sel]) gift_sel = (gift_sel + 1) % NITEMS; }
            else { mode = M_WALK; talk_v = -1; }
        }
        break;
    }
    case M_GIFT:
        if (any_repeat(BTN_RIGHT | BTN_DOWN)) { do gift_sel = (gift_sel + 1) % NITEMS; while (!inv[gift_sel]); sfx(SFX_MOVE); }
        if (any_repeat(BTN_LEFT | BTN_UP)) { do gift_sel = (gift_sel + NITEMS - 1) % NITEMS; while (!inv[gift_sel]); sfx(SFX_MOVE); }
        if (any_pressed(BTN_A)) give(talk_v, gift_sel);
        else if (any_pressed(BTN_B)) { mode = M_MENU; sfx(SFX_BACK); }
        break;
    case M_FISH:
        fish_t++;
        if (fish_state == 1 && fish_t >= fish_wait) { fish_state = 2; fish_t = 0; sfx(SFX_DING); }
        if (fish_state == 2 && fish_t > 32) {
            fish_state = 0;
            mode = M_TALK;
            talk_v = -1;
            dlg_push(DLG_NARRATOR, 0, "It got away.");
            platform_log("village: a fish got away");
        }
        if (any_pressed(BTN_A) && fish_state == 1 && fish_t > 10) {
            fish_state = 0; mode = M_TALK; talk_v = -1;
            dlg_push(DLG_NARRATOR, 0, "Too early. The ripples fade.");
        } else if (any_pressed(BTN_A) && fish_state == 2) {
            fish_state = 0;
            save.fish_caught = (u8)imin(255, save.fish_caught + 1);
            platform_log("village: caught a fish");
            inv[IT_FISH] = imin(9, inv[IT_FISH] + 1);
            sfx(SFX_GOAL);
            mode = M_TALK;
            talk_v = -1;
            dlg_push(DLG_NARRATOR, 0, "You caught a fish!");
            if (iabs(px - vil[4].x) < 70) {
                str_int(fish_n, save.fish_caught);
                chat_say("Kuro", vills[4].color, vills[4].voice, SPK_KURO, SIT_V_GOSSIP_FISH, "n", fish_n, 2);
            }
        } else if (any_pressed(BTN_B)) {
            fish_state = 0;
            mode = M_WALK;
        }
        break;
    case M_SLEEP:
        if (any_pressed(BTN_A)) {
            new_day();
            sfx(SFX_OK);
            mode = M_TALK;
            talk_v = -1;
            dlg_push(DLG_NARRATOR, 0, "You sleep until morning. The meadow has fresh daisies.");
        } else if (any_pressed(BTN_B)) {
            mode = M_WALK;
        }
        break;
    case M_WALK: {
        int dx = 0, dy = 0;
        if (held(BTN_LEFT)) dx = -1;
        if (held(BTN_RIGHT)) dx = 1;
        if (held(BTN_UP)) dy = -1;
        if (held(BTN_DOWN)) dy = 1;
        int step = frame_count % 3 ? 1 : 2;
        if (dx) pdir = dx;
        for (int s = 0; s < step; s++) {
            if (dx && !blocked(px + dx, py)) px += dx;
            if (dy && !blocked(px, py + dy)) py += dy;
        }
        if (dx || dy) pwalk++;
        if (any_pressed(BTN_A)) try_action();
        if (any_pressed(BTN_START)) { sfx(SFX_BACK); scene_fade_to(&scene_menu); }
        /* someone nearby remarks on the day now and then */
        if (--ambient_t <= 0) {
            ambient_t = 60 * rnd_range(14, 24);
            int v = near_villager();
            if (v < 0) {
                int bd = 80;
                for (int i = 0; i < NV; i++) {
                    int d = iabs(vil[i].x - px) + iabs(vil[i].y - py);
                    if (d < bd) { bd = d; v = i; }
                }
            }
            if (v >= 0)
                chat_say(vills[v].name, vills[v].color,
                         vills[v].cast_id >= 0 ? cast[vills[v].cast_id].voice : vills[v].voice,
                         vills[v].spk, night_level() > 100 ? SIT_V_NIGHT : SIT_V_SMALLTALK, 0, 0, 1);
        }
        break;
    }
    }
}

/* ---------------------------------------------------------------- draw */
static void draw_people(void)
{
    int order[NV + 1], n = 0;
    for (int v = 0; v < NV; v++) order[n++] = v;
    order[n++] = NV;                                          /* you */
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0; j--) {
            int ya = order[j] == NV ? py : vil[order[j]].y, yb = order[j - 1] == NV ? py : vil[order[j - 1]].y;
            if (ya >= yb) break;
            int t = order[j]; order[j] = order[j - 1]; order[j - 1] = t;
        }
    for (int i = 0; i < n; i++) {
        int who = order[i];
        if (who == NV) {
            chibi(LK_HIRE, px, py - 24, CE_NORMAL, (mode == M_WALK && (btn_held(0, BTN_DIRS) || btn_held(1, BTN_DIRS)))
                                                     ? 1 + (pwalk / 6) % 3 : 0, pdir > 0);
            continue;
        }
        const Villager *w = &vil[who];
        int moving = w->x != w->tx || w->y != w->ty;
        int expr = (mode != M_WALK && who == talk_v) ? CE_HAPPY : (who == 4 ? CE_SLEEPY : CE_NORMAL);
        chibi(vills[who].look, w->x, w->y - 24, expr, moving ? 1 + (w->walk / 6) % 3 : 0, w->dir > 0);
        if (hearts(who) >= 30)
            text(w->x - 2, w->y - 44, "\x01", C_PINK);
    }
}

static void draw_hud(void)
{
    rect(0, 0, SCREEN_W, 12, C_INK);
    char buf[24];
    str_int(str_cpy(buf, "Day "), save.village_days);
    text(4, 2, buf, C_SKY);
    static const char *const parts[5] = { "morning", "afternoon", "evening", "dusk", "night" };
    int q = day_t >= DAY_LEN ? 4 : day_t * 4 / DAY_LEN;
    if (q >= 3) {                                            /* moon */
        circle_fill(51, 6, 4, C_GOLD);
        circle_fill(53, 5, 3, C_INK);
    } else {                                                 /* sun */
        circle_fill(51, 6, 3, C_ORANGE);
        for (int a = 0; a < 8; a++) pset(51 + icos(a * 32) * 5 / 256, 6 + isin(a * 32) * 5 / 256, C_GOLD);
    }
    text(60, 2, parts[q], C_GREY);
    for (int i = 0, x = 196; i < NITEMS; i++, x += 30) {
        draw_item_icon(i, x, 2);
        str_int(buf, inv[i]);
        text(x + 11, 2, buf, inv[i] ? C_WHITE : C_DIM);
    }
}

static void draw_portrait(int v)
{
    if (vills[v].cast_id >= 0) {
        draw_bust(vills[v].cast_id, dlg_expr(), 6, 62, 1);
    } else {
        round_box(10, 76, 50, 50, vills[v].color, RGB(250, 240, 230));
        chibi_bust(vills[v].look, 35, 98, CE_HAPPY);
    }
}

static void vil_draw(void)
{
    draw_map();
    draw_people();
    if (mode == M_FISH) {                                     /* line and bobber */
        int bx = px + pdir * 22, by = py - 2 + (fish_state == 2 ? 2 : (int)(frame_count / 20) % 2);
        line(px + pdir * 6, py - 18, bx, by, C_WHITE);
        circle_fill(bx, by, 2, C_RED);
        if (fish_state == 2) text(bx - 2, by - 14, "!", C_GOLD);
    }
    int nl = night_level();
    if (nl) {
        rect_blend(0, 12, SCREEN_W, SCREEN_H - 12, RGB(20, 24, 70), nl);
        for (int i = 0; i < 2; i++) {                         /* lamp light pools */
            int lx = i ? 216 : 118;
            rect_blend(lx - 12, 70, 24, 24, C_GOLD, nl / 5);
        }
        rect_blend(18, 36, 10, 8, C_GOLD, nl);
    }
    draw_hud();
    if (mode == M_WALK) {
        int v = near_villager();
        if (v >= 0) {
            char b[40];
            str_cpy(str_cpy(b, "^3A^0 talk to "), vills[v].name);
            text_sh(iclamp(px - 30, 2, SCREEN_W - 80), imax(14, py - 50), b, C_WHITE, C_INK);
        }
        if (chat_showing()) rect_blend(0, 160, SCREEN_W, 20, C_INK, 120);
        chat_draw(4, 162, 312);
    }
    if (mode == M_TALK || mode == M_MENU || mode == M_GIFT) {
        if (talk_v >= 0) draw_portrait(talk_v);
        dlg_draw(4, 128, 312, 48);
    }
    if (mode == M_MENU || mode == M_GIFT) {
        int v = talk_v;
        round_box(196, 70, 116, 54, vills[v].color, RGB(36, 30, 58));
        text(202, 74, vills[v].name, vills[v].color);
        for (int h = 0; h < 5; h++)
            spr(hearts(v) >= (h + 1) * 20 ? SPR_HEART : SPR_HEART_EMPTY, 246 + h * 12, 73, 0);
        if (mode == M_MENU) {
            int has = inv[0] + inv[1] + inv[2] + inv[3];
            const char *opts[3] = { "Chat", has ? "Give a gift" : "Bye", has ? "Bye" : 0 };
            for (int i = 0; i < 3 && opts[i]; i++) {
                int y = 88 + i * 11;
                if (i == menu_sel) text(202, y, "\x05", C_GOLD);
                text(212, y, opts[i], i == menu_sel ? C_WHITE : C_GREY);
            }
        } else {
            text(202, 88, "Give which?", C_GREY);
            draw_item_icon(gift_sel, 202, 101);
            text(214, 101, item_names[gift_sel], C_WHITE);
            text(202, 112, "^3<>^0 choose  ^3A^0 give", C_DIM);
        }
    }
    if (mode == M_SLEEP) {
        round_box(80, 70, 160, 40, C_SKY, C_PANEL);
        text_center(SCREEN_W / 2, 78, "Sleep until morning?", C_WHITE, C_INK);
        text_center(SCREEN_W / 2, 94, "^3A^0 sleep   ^3B^0 not yet", C_GREY, C_INK);
    }
    if (mode == M_TALK && talk_v < 0)
        dlg_draw(4, 128, 312, 48);
}

const Scene scene_village = { vil_enter, vil_update, vil_draw, "village" };
