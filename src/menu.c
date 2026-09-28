/* Nia's desk: the hub. Her builds are listed on her monitor; she comments live (NekoLM, on the device)
 * on whichever build you highlight and on how your last playtest went. */
#include "engine.h"
#include "save.h"
#include "cast.h"
#include "nekolm.h"
#include "lines.h"
#include "hub.h"
#include "live.h"

static int sel, idle_t, visits;
static int talk_live, talk_sit, talk_fixed;
static char talk_text[128];
static int pending_report = -1;
static int nia_expr;

void hub_report(int sit) { pending_report = sit; }

static const struct { const char *title, *tag; int sit; u16 color; } builds[6] = {
    { "Rival!", "Mako learns how you shoot", SIT_SEL_RIVAL, C_PINK },
    { "Whisker Detective", "AI-written cases with Shio", SIT_SEL_DETECTIVE, C_MINT },
    { "Nyan Cafe", "rush hour at Mocha's, co-op", SIT_SEL_CAFE, C_ORANGE },
    { "Foster Kittens", "kittens with learning brains", SIT_SEL_KITTENS, C_GOLD },
    { "Nekomura", "the village remembers you", SIT_SEL_VILLAGE, C_SKY },
    { "Thin Walls", "a dating sim: Nia, Mako, Shio", -1, C_ROSE },
};

/* NekoLM has no situation for build 06 yet (it needs new lines and a retrain), so here Nia says one of these,
 * written in her card's voice, and the tag under her line says so. */
static const char *const date_lines[3] = {
    "Build six. A dating sim. I'm one of the routes. Don't ask.",
    "The tail animation took eleven hours. It was a hard problem.",
    "It's for testing. The save data is 48 bytes. Efficient.",
};

static LiveJob talk;

static void nia_say(int sit, int expr)
{
    if (sit < 0) {                                   /* build 06: a fixed line */
        static int k;
        str_cpy(talk_text, date_lines[k++ % 3]);
        talk_live = 0;
        talk_fixed = 1;
        nia_expr = EX_FLUSTERED;
        return;
    }
    talk_fixed = 0;
    talk_sit = sit;
    talk_live = 1;
    talk_text[0] = 0;
    nia_expr = expr;
    live_init(&talk, SPK_NIA, sit, 0, 0);
}

static void menu_enter(void)
{
    music(MUS_MENU);
    idle_t = 0;
    if (pending_report >= 0) {
        int happy = pending_report == SIT_AFTER_WIN_RIVAL || pending_report == SIT_AFTER_CASE;
        nia_say(pending_report, happy ? EX_HAPPY : EX_TIRED);
        pending_report = -1;
    } else {
        nia_say(visits ? SIT_HUB_BACK : SIT_HUB_HELLO, EX_NEUTRAL);
    }
    visits++;
}

static void menu_update(void)
{
    idle_t++;
    if (talk_live && live_step(&talk, 4000, 64)) {
        talk_live = 0;
        str_cpy(talk_text, talk.out);
    }
    int moved = 0;
    if (any_repeat(BTN_DOWN)) { sel = (sel + 1) % 6; moved = 1; }
    if (any_repeat(BTN_UP)) { sel = (sel + 5) % 6; moved = 1; }
    if (moved) {
        sfx(SFX_MOVE);
        idle_t = 0;
        nia_say(builds[sel].sit, EX_NEUTRAL);
    }
    if (idle_t == 60 * 25)
        nia_say(SIT_IDLE, EX_TIRED);
    if (any_pressed(BTN_A)) {
        static const Scene *const scenes[6] = { &scene_rival, &scene_detective, &scene_cafe, &scene_kittens, &scene_village,
                                                &scene_date };
        sfx(SFX_OK);
        scene_fade_to(scenes[sel]);
    }
}

/* ---------------------------------------------------------------- Nia's room */
static void draw_room(void)
{
    gradient_v(0, 0, SCREEN_W, SCREEN_H, RGB(46, 34, 72), RGB(22, 18, 38));
    /* window with the night sky */
    int wx = 186, wy = 8, ww = 126, wh = 92;
    gradient_v(wx, wy, ww, wh, RGB(24, 30, 70), RGB(70, 60, 120));
    for (int i = 0; i < 26; i++) {
        u32 h = (u32)i * 2654435761u;
        int sx = wx + 3 + (int)(h % (u32)(ww - 6)), sy = wy + 3 + (int)((h >> 12) % (u32)(wh - 30));
        int tw = (int)((frame_count / 7 + h) % 40);
        pset(sx, sy, tw < 3 ? C_WHITE : blend(C_GREY, RGB(40, 44, 90), 100));
    }
    circle_fill(wx + 96, wy + 22, 10, RGB(255, 244, 214));
    circle_fill(wx + 101, wy + 19, 9, RGB(28, 34, 76));
    rect_line(wx - 2, wy - 2, ww + 4, wh + 4, RGB(120, 84, 60));
    vline(wx + ww / 2, wy, wh, RGB(120, 84, 60));
    hline(wx, wy + wh / 2, ww, RGB(120, 84, 60));
    /* curtains */
    for (int k = 0; k < 12; k++) {
        vline(wx - 4 + k / 3, wy - 4, wh + 14 - k, RGB(150, 90, 150));
        vline(wx + ww + 3 - k / 3, wy - 4, wh + 14 - k, RGB(150, 90, 150));
    }
    /* string lights across the top */
    for (int i = 0; i < 16; i++) {
        int lx = 8 + i * 20, ly = 4 + (i % 2);
        static const u16 cols[4] = { C_PINK, C_GOLD, C_MINT, C_SKY };
        u16 c = cols[i % 4];
        int on = ((frame_count / 20 + (u32)i) % 5) != 0;
        if (on) { circle_fill(lx, ly + 2, 3, blend(RGB(46, 34, 72), c, 90)); }
        circle_fill(lx, ly + 2, 1, on ? c : blend(c, C_INK, 150));
    }
    hline(0, 4, SCREEN_W, RGB(60, 48, 80));
    /* desk */
    rect(0, 156, SCREEN_W, 24, RGB(120, 84, 60));
    hline(0, 156, SCREEN_W, RGB(160, 118, 84));
    /* energy drink cans and a mug by the keyboard */
    for (int i = 0; i < 3; i++) {
        int cx = 150 + i * 9 - (i == 2 ? 30 : 0);
        rect(cx, 146, 6, 11, i == 1 ? C_MINT : RGB(90, 130, 255));
        hline(cx, 148, 6, C_WHITE);
    }
}

static void menu_draw(void)
{
    draw_room();
    /* Nia herself */
    draw_bust(CH_NIA, nia_expr, 190, 54, 2);
    /* her monitor with the builds */
    round_box(6, 14, 178, 128, RGB(106, 168, 255), RGB(20, 24, 48));
    rect_blend(8, 16, 174, 124, RGB(60, 90, 200), 40);
    text_sh(12, 18, "NIA'S BUILDS  v0.1", C_SKY, C_INK);
    text(140, 18, "\x04 play", C_DIM);
    for (int i = 0; i < 6; i++) {
        int y = 30 + i * 18;
        if (i == sel) {
            round_box(9, y - 2, 172, 18, builds[i].color, RGB(44, 44, 86));
            spr(SPR_PAW, 12, y + 3, 0);
        }
        char num[4] = { '0', (char)('1' + i), 0, 0 };
        text(26, y, num, builds[i].color);
        text_sh(40, y, builds[i].title, i == sel ? C_WHITE : C_GREY, C_INK);
        text(40, y + 8, builds[i].tag, i == sel ? builds[i].color : C_DIM);
    }
    /* what Nia says, written live on the device */
    if (talk_text[0] || talk_live) {
        round_box(6, 146, 178, 32, C_WHITE, RGB(36, 30, 58));
        round_box(12, 140, 26, 11, C_WHITE, cast[CH_NIA].name_color);
        text(16, 142, "Nia", C_INK);
        int starts[3], lens[3];
        int n = talk_live ? 0 : wrap_text(talk_text, 166, starts, lens, 2);
        for (int i = 0; i < n; i++)
            text_n(12, 153 + i * 10, talk_text + starts[i], lens[i], C_WHITE);
        for (int i = 0; talk_live && i < 1 + (int)(frame_count / 10) % 3; i++)
            circle_fill(16 + i * 7, 158, 2, C_GREY);
        text(128, 142, talk_live ? "\x02 thinking" : talk_fixed ? "\x02 set line" : "\x02 NekoLM", talk_live ? C_GOLD : C_DIM);
    }
}

const Scene scene_menu = { menu_enter, menu_update, menu_draw, "hub" };
