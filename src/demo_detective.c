/* Build 2 - Whisker Detective: small mysteries in the apartment building, solved with Shio. The cases were
 * written by the teacher model and kept only when a logic checker proved them fair; Shio's remarks are
 * written live by NekoLM. Search the kitchen for clues, then catch the one statement a clue disproves. */
#include "engine.h"
#include "save.h"
#include "cast.h"
#include "dialog.h"
#include "live.h"
#include "nekolm.h"
#include "cases.h"
#include "hub.h"

enum { ST_LIST, ST_BRIEF, ST_SEARCH, ST_TESTIMONY, ST_PRESENT, ST_REACT, ST_GOTCHA, ST_WRAP, ST_RESULT };

static int state, state_t, fade_t, fade_to = -1;
static int list_sel, case_i;
static const Case *cs;
static int found[4], nfound, sel_clue, cur_x, cur_y;
static int mk_x[4], mk_y[4], order[4];
static int st_sel, ev_sel, paws, mistakes, odd_said, notebook, paused;
static int idle_t, shake, react_ok, suspect_expr, toast_t, toast_clue;
static int after = -1;               /* state to fade to once the dialogue queue empties */

static const int suspects[3] = { CH_NIA, CH_MAKO, CH_MOCHA };

static void shio(int sit, int expr) { dlg_push_live(CH_SHIO, expr, SPK_SHIO, sit, 0, 0); }

static void go(int s)
{
    state = s;
    state_t = 0;
}

static void fade(int s)
{
    fade_to = s;
    fade_t = 1;
}

/* ---------------------------------------------------------------- the kitchen */
static const i16 spot_xy[SPOT_COUNT][2] = {
    { 28, 74 }, { 67, 56 }, { 102, 76 }, { 144, 76 }, { 208, 100 }, { 150, 122 },
    { 203, 124 }, { 277, 124 }, { 68, 114 }, { 100, 160 }, { 268, 62 },
};

static void place_markers(void)
{
    for (int i = 0; i < cs->nclues; i++) {
        int s = cs->clues[i].spot, twin = 0;
        for (int j = 0; j < i; j++)
            if (cs->clues[j].spot == s) twin++;
        mk_x[i] = spot_xy[s][0] + (s == SPOT_PANTRY ? 0 : twin * 20);
        mk_y[i] = spot_xy[s][1] + (s == SPOT_PANTRY ? twin * 28 : 0);
        order[i] = i;
    }
    for (int i = 1; i < cs->nclues; i++)          /* left-to-right order for the d-pad */
        for (int j = i; j > 0 && mk_x[order[j]] < mk_x[order[j - 1]]; j--) {
            int t = order[j]; order[j] = order[j - 1]; order[j - 1] = t;
        }
}

static void draw_kitchen(int cafe)
{
    u16 wall_top = cafe ? RGB(250, 236, 212) : RGB(252, 234, 224);
    u16 wall_bot = cafe ? RGB(236, 212, 184) : RGB(238, 208, 216);
    gradient_v(0, 14, SCREEN_W, 98, wall_top, wall_bot);
    if (cafe)
        for (int y = 80; y < 108; y += 7)
            for (int x = (y / 7) % 2 * 7; x < SCREEN_W; x += 14)
                rect(x, y, 7, 7, RGB(170, 210, 182));
    /* floor */
    if (cafe) {
        rect(0, 112, SCREEN_W, 68, RGB(238, 230, 216));
        for (int y = 112; y < SCREEN_H; y += 9)
            for (int x = ((y - 112) / 9) % 2 * 9; x < SCREEN_W; x += 18)
                rect(x, y, 9, 9, RGB(92, 80, 96));
    } else {
        rect(0, 112, SCREEN_W, 68, RGB(204, 150, 110));
        for (int r = 0, y = 112; y < SCREEN_H; r++, y += 8) {
            hline(0, y, SCREEN_W, RGB(164, 114, 84));
            for (int x = (r * 37) % 60; x < SCREEN_W; x += 60)
                vline(x, y, 8, RGB(172, 122, 90));
            if (r % 2) rect_blend(0, y + 1, SCREEN_W, 7, RGB(190, 136, 100), 90);
        }
    }
    rect(0, 108, SCREEN_W, 4, cafe ? RGB(150, 110, 84) : RGB(214, 170, 180));
    /* morning light from the window */
    rect_blend(112, 113, 76, 40, C_WHITE, 36);
    /* window */
    gradient_v(122, 24, 52, 38, RGB(150, 206, 255), RGB(255, 232, 206));
    circle_fill(162, 34, 5, RGB(255, 250, 220));
    rect_line(121, 23, 54, 40, C_WHITE);
    vline(148, 24, 38, C_WHITE);
    rect(118, 62, 60, 3, C_WHITE);
    rect(126, 54, 8, 8, RGB(214, 120, 84));
    circle_fill(128, 51, 3, RGB(96, 176, 110));
    circle_fill(133, 50, 3, RGB(120, 196, 124));
    /* clock */
    circle_fill(100, 38, 7, C_WHITE);
    circle_line(100, 38, 7, C_INK);
    line(100, 38, 100, 33, C_INK);
    line(100, 38, 104, 40, C_ROSE);
    /* fridge */
    u16 fr_edge = RGB(150, 160, 182), fr = cafe ? RGB(206, 212, 222) : RGB(236, 244, 250);
    round_box(10, 34, 36, 82, fr_edge, fr);
    hline(11, 58, 34, fr_edge);
    rect(40, 40, 2, 12, RGB(170, 178, 196));
    rect(40, 64, 2, 22, RGB(170, 178, 196));
    if (!cafe) {
        circle_fill(18, 44, 2, C_PINK);
        circle_fill(30, 46, 2, C_GOLD);
        rect(16, 66, 12, 9, C_WHITE);
        rect(17, 67, 10, 5, RGB(170, 214, 255));
        circle_fill(22, 90, 2, C_MINT);
    }
    /* pantry */
    u16 pa = cafe ? RGB(160, 112, 84) : RGB(226, 200, 164), pa_edge = cafe ? RGB(110, 76, 58) : RGB(176, 146, 112);
    round_box(52, 28, 30, 84, pa_edge, pa);
    for (int y = 36; y < 70; y += 4)
        hline(57, y, 20, pa_edge);
    circle_fill(77, 74, 1, C_GOLD);
    /* upper cabinets */
    u16 cab = cafe ? RGB(150, 104, 80) : RGB(174, 216, 202), cab_edge = cafe ? RGB(104, 70, 54) : RGB(112, 156, 146);
    round_box(180, 22, 52, 30, cab_edge, cab);
    vline(206, 23, 28, cab_edge);
    pset(202, 44, C_INK);
    pset(210, 44, C_INK);
    /* counter */
    rect(86, 90, 146, 26, cab);
    rect_line(86, 90, 146, 26, cab_edge);
    for (int x = 122; x < 232; x += 36)
        vline(x, 90, 26, cab_edge);
    for (int x = 104; x < 186; x += 36)
        rect(x, 100, 4, 2, cab_edge);
    rect(84, 84, 150, 6, cafe ? RGB(96, 86, 92) : RGB(250, 250, 244));
    hline(84, 89, 150, cafe ? RGB(60, 52, 58) : RGB(200, 196, 190));
    /* sink and tap */
    rect(128, 84, 38, 2, RGB(150, 160, 172));
    vline(146, 72, 12, RGB(170, 180, 192));
    hline(146, 72, 7, RGB(170, 180, 192));
    pset(152, 73, RGB(170, 180, 192));
    /* oven with its timer */
    round_box(188, 91, 40, 24, RGB(120, 124, 140), RGB(214, 218, 228));
    rect(200, 93, 16, 5, RGB(30, 40, 44));
    rect(202, 94, 3, 3, C_MINT);
    rect(208, 94, 5, 3, C_MINT);
    rect(194, 101, 28, 10, RGB(60, 60, 78));
    hline(196, 103, 12, RGB(96, 96, 120));
    /* kettle */
    circle_fill(208, 78, 6, cafe ? RGB(80, 150, 120) : C_PINK);
    rect(203, 81, 11, 3, cafe ? RGB(80, 150, 120) : C_PINK);
    line(214, 76, 218, 73, C_INK);
    if (cafe) {                                    /* espresso machine */
        round_box(90, 62, 22, 22, RGB(120, 120, 136), RGB(200, 204, 214));
        rect(94, 70, 14, 4, RGB(60, 60, 78));
        rect(98, 78, 6, 5, C_WHITE);
    }
    /* door */
    round_box(252, 30, 32, 82, RGB(120, 84, 60), RGB(190, 140, 108));
    rect_line(257, 36, 22, 30, RGB(160, 116, 88));
    rect_line(257, 72, 22, 34, RGB(160, 116, 88));
    circle_fill(258, 72, 2, C_GOLD);
    /* table with a mug */
    rect(112, 130, 72, 5, cafe ? RGB(120, 84, 64) : RGB(222, 172, 126));
    rect(118, 135, 2, 26, RGB(150, 104, 78));
    rect(176, 135, 2, 26, RGB(150, 104, 78));
    rect(158, 124, 7, 6, C_WHITE);
    pset(165, 126, C_WHITE);
    /* chair */
    rect(210, 114, 4, 32, RGB(214, 132, 110));
    rect(192, 140, 22, 4, RGB(234, 150, 126));
    rect(194, 144, 2, 18, RGB(180, 110, 92));
    rect(210, 144, 2, 18, RGB(180, 110, 92));
    /* couch (a booth in the cafe) */
    u16 co = cafe ? RGB(96, 152, 116) : RGB(222, 128, 166), co2 = cafe ? RGB(130, 186, 146) : RGB(244, 172, 202);
    round_box(238, 116, 78, 24, blend(co, C_INK, 60), co);
    rect(236, 136, 82, 14, co2);
    round_box(232, 126, 10, 26, blend(co, C_INK, 60), co);
    round_box(312, 126, 8, 26, blend(co, C_INK, 60), co);
    if (!cafe) {
        round_box(246, 120, 26, 16, C_WHITE, co2);
        round_box(280, 120, 26, 16, C_WHITE, RGB(250, 214, 150));
    }
    rect(240, 150, 3, 6, C_INK);
    rect(312, 150, 3, 6, C_INK);
    /* trash can */
    round_box(58, 126, 20, 24, RGB(80, 140, 124), RGB(130, 196, 174));
    rect(56, 122, 24, 4, RGB(96, 160, 140));
    rect(64, 150, 8, 2, C_INK);
}

static void draw_magnifier(int x, int y)
{
    circle_line(x, y, 8, C_INK);
    circle_line(x, y, 7, C_WHITE);
    circle_line(x, y, 6, C_INK);
    pset(x - 3, y - 3, C_WHITE);
    pset(x - 2, y - 4, C_WHITE);
    for (int i = 0; i < 3; i++)
        line(x + 6 + i / 2, y + 6 + (i + 1) / 2, x + 11 + i / 2, y + 11 + (i + 1) / 2, i == 1 ? RGB(150, 104, 78) : C_INK);
}

static void draw_markers(void)
{
    for (int i = 0; i < cs->nclues; i++) {
        int x = mk_x[i], y = mk_y[i];
        if (found[i]) {
            circle_fill(x, y, 5, C_MINT);
            text(x - 3, y - 4, "\x04", C_INK);
        } else {
            int bob = isin((int)(frame_count * 4 + (u32)i * 60)) / 90;
            round_box(x - 5, y - 7 + bob, 11, 12, C_ROSE, C_WHITE);
            text(x - 2, y - 5 + bob, "?", C_ROSE);
        }
    }
}

/* ---------------------------------------------------------------- shared pieces */
static void draw_paws(int x, int y)
{
    for (int i = 0; i < 3; i++) {
        if (i < paws)
            spr(SPR_PAW, x + i * 13, y, 0);
        else
            spr_tinted(SPR_PAW, x + i * 13, y, 0, 0, C_PANEL, 200);
    }
}

static void draw_header(const char *label)
{
    rect(0, 0, SCREEN_W, 14, C_INK);
    text(6, 3, label, C_MINT);
    text(12 + text_w(label), 3, cs->title, C_WHITE);
    draw_paws(SCREEN_W - 44, 3);
}

static void draw_wrapped(int x, int y, int w, const char *s, u16 c, int maxlines)
{
    int starts[6], lens[6];
    int n = wrap_text(s, w, starts, lens, imin(maxlines, 6));
    for (int i = 0; i < n; i++)
        text_n(x, y + i * LINE_H, s + starts[i], lens[i], c);
}

/* the evidence list: used for the notebook (read only) and for presenting */
static void draw_evidence(const char *title, int presenting)
{
    rect_blend(0, 14, SCREEN_W, SCREEN_H - 14, C_NIGHT, 150);
    round_box(14, 22, 292, 118, C_MINT, RGB(30, 40, 56));
    text_sh(22, 28, title, C_MINT, C_INK);
    for (int i = 0; i < cs->nclues; i++) {
        int y = 44 + i * 16;
        if (i == ev_sel) {
            round_box(20, y - 3, 116, 14, C_MINT, RGB(52, 74, 90));
            spr(SPR_PAW, 23, y - 1, 0);
        }
        text(36, y, found[i] ? cs->clues[i].name : "???", i == ev_sel ? C_WHITE : C_GREY);
    }
    vline(142, 40, 94, RGB(70, 90, 110));
    if (found[ev_sel])
        draw_wrapped(150, 44, 148, cs->clues[ev_sel].desc, C_WHITE, 6);
    else
        text(150, 44, "Not found yet.", C_DIM);
    text(22, 126, presenting ? "^3A^0 present   ^3B^0 cancel" : "^3B^0 close", C_GREY);
}

static void draw_portrait(int x, int y, int scale, int who, int expr)
{
    if (who < 0)
        return;
    draw_bust(who, expr, x, y, scale);
}

/* ---------------------------------------------------------------- case flow */
static void start_case(int i)
{
    case_i = i;
    cs = &cases[i];
    for (int k = 0; k < 4; k++) found[k] = 0;
    nfound = 0;
    sel_clue = 0;
    st_sel = 0;
    ev_sel = 0;
    paws = 3;
    mistakes = 0;
    odd_said = 0;
    notebook = 0;
    paused = 0;
    idle_t = 0;
    suspect_expr = EX_NEUTRAL;
    place_markers();
    sel_clue = order[0];
    cur_x = mk_x[sel_clue];
    cur_y = mk_y[sel_clue];
    dlg_clear();
    shio(SIT_D_INTRO, EX_NEUTRAL);
    dlg_push(CH_SHIO, EX_NEUTRAL, cs->intro);
    dlg_push(DLG_NARRATOR, 0, "Look around. ^3Left/Right^0 moves the lens, ^3A^0 examines, ^3Y^0 opens the notebook.");
    after = ST_SEARCH;
    fade(ST_BRIEF);
}

static void examine(int i)
{
    sfx(SFX_DING);
    if (!found[i]) {
        found[i] = 1;
        nfound++;
        toast_clue = i;
        toast_t = 150;
    }
    char *b = dlg_buf();
    str_cpy(str_cpy(str_cpy(str_cpy(b, "^2"), cs->clues[i].name), "^0 - "), cs->clues[i].desc);
    dlg_push(DLG_NARRATOR, 0, b);
    dlg_push_live(CH_SHIO, EX_NEUTRAL, SPK_SHIO, SIT_D_EVIDENCE, "item", cs->clues[i].name);
    if (nfound == cs->nclues) {
        dlg_push(CH_SHIO, EX_SMUG, "That is everything in this room. Now we hear their stories.");
        dlg_push(DLG_NARRATOR, 0, "One statement is a lie a clue can disprove. ^3A^0 presents evidence against it.");
        after = ST_TESTIMONY;
    }
}

static void present(void)
{
    react_ok = cs->st[st_sel].contra == ev_sel;
    go(ST_REACT);
}

static void judge(void)
{
    if (react_ok) {
        sfx(SFX_GOAL);
        shake = 16;
        go(ST_GOTCHA);
        return;
    }
    sfx(SFX_BUZZ);
    shake = 10;
    mistakes++;
    save.wrong_accusations++;
    {
        char *b = log_buf(), *q = str_cpy(b, "detective: wrong evidence '");
        q = str_cpy(q, cs->clues[ev_sel].name);
        q = str_cpy(q, "' against statement ");
        str_int(q, st_sel + 1);
        platform_log(b);
    }
    paws--;
    suspect_expr = EX_SMUG;
    shio(SIT_D_WRONG, EX_ANNOYED);
    if (paws == 1) {
        const Statement *lie = 0;
        for (int i = 0; i < cs->nst; i++)
            if (cs->st[i].contra >= 0) lie = &cs->st[i];
        char *b = dlg_buf();
        str_cpy(str_cpy(str_cpy(str_cpy(str_cpy(b, "^3Hint:^0 compare "), cast[lie->who].name),
                                "'s story with the ^2"), cs->clues[lie->contra].name), "^0.");
        shio(SIT_D_HINT, EX_NEUTRAL);
        dlg_push(DLG_NARRATOR, 0, b);
    } else if (paws == 0) {
        dlg_push(CH_SHIO, EX_TIRED, "The trail is going cold. One more try. Focus.");
        paws = 1;
    }
    after = ST_TESTIMONY;
    go(ST_TESTIMONY);
}

static void solved(void)
{
    {
        char *b = log_buf(), *q = str_cpy(b, "detective: solved '");
        q = str_cpy(q, cs->title);
        q = str_cpy(q, "' with ");
        q = str_int(q, paws);
        q = str_cpy(q, " paws, ");
        q = str_int(q, mistakes);
        str_cpy(q, " wrong");
        platform_log(b);
    }
    if (save.case_done[case_i] < paws)
        save.case_done[case_i] = (u8)paws;
    int n = 0;
    for (int i = 0; i < case_count && i < 16; i++)
        n += save.case_done[i] > 0;
    save.cases_solved = (u8)n;
    hub_report(SIT_AFTER_CASE);
    dlg_clear();
    shio(SIT_D_CONTRA, EX_SMUG);
    dlg_push(cs->culprit, EX_FLUSTERED, cs->confession);
    dlg_push(CH_SHIO, EX_HAPPY, cs->wrapup);
    shio(SIT_D_SOLVED, EX_SPARKLE);
    after = ST_RESULT;
    go(ST_WRAP);
}

/* ---------------------------------------------------------------- update */
static void det_enter(void)
{
    music(MUS_MYSTERY);
    dlg_clear();
    live_ban_he = 1;
    after = -1;
    fade_t = 0;
    go(ST_LIST);
}

static void leave(void)
{
    dlg_clear();
    live_ban_he = 0;
    scene_fade_to(&scene_menu);
}

static void update_list(void)
{
    if (any_repeat(BTN_DOWN)) { list_sel = (list_sel + 1) % case_count; sfx(SFX_MOVE); }
    if (any_repeat(BTN_UP)) { list_sel = (list_sel + case_count - 1) % case_count; sfx(SFX_MOVE); }
    if (any_pressed(BTN_A)) { sfx(SFX_OK); start_case(list_sel); }
    else if (any_pressed(BTN_B)) { sfx(SFX_BACK); leave(); }
}

static void update_search(void)
{
    if (notebook) {
        if (any_repeat(BTN_DOWN)) { ev_sel = (ev_sel + 1) % cs->nclues; sfx(SFX_MOVE); }
        if (any_repeat(BTN_UP)) { ev_sel = (ev_sel + cs->nclues - 1) % cs->nclues; sfx(SFX_MOVE); }
        if (any_pressed(BTN_B | BTN_Y)) { notebook = 0; sfx(SFX_BACK); }
        return;
    }
    int pos = 0;
    for (int k = 0; k < cs->nclues; k++)
        if (order[k] == sel_clue) pos = k;
    if (any_repeat(BTN_RIGHT | BTN_DOWN)) { pos = (pos + 1) % cs->nclues; sfx(SFX_STEP); }
    if (any_repeat(BTN_LEFT | BTN_UP)) { pos = (pos + cs->nclues - 1) % cs->nclues; sfx(SFX_STEP); }
    sel_clue = order[pos];
    int dx = mk_x[sel_clue] - cur_x, dy = mk_y[sel_clue] - cur_y;
    cur_x += iabs(dx) < 3 ? dx : dx / 3;
    cur_y += iabs(dy) < 3 ? dy : dy / 3;
    if (any_pressed(BTN_A)) examine(sel_clue);
    else if (any_pressed(BTN_Y)) { notebook = 1; ev_sel = sel_clue; sfx(SFX_POP); }
}

static void update_testimony(void)
{
    if (notebook) {
        if (any_repeat(BTN_DOWN)) { ev_sel = (ev_sel + 1) % cs->nclues; sfx(SFX_MOVE); }
        if (any_repeat(BTN_UP)) { ev_sel = (ev_sel + cs->nclues - 1) % cs->nclues; sfx(SFX_MOVE); }
        if (any_pressed(BTN_B | BTN_Y)) { notebook = 0; sfx(SFX_BACK); }
        return;
    }
    int moved = 0;
    if (any_repeat(BTN_RIGHT | BTN_DOWN)) { st_sel = (st_sel + 1) % cs->nst; moved = 1; }
    if (any_repeat(BTN_LEFT | BTN_UP)) { st_sel = (st_sel + cs->nst - 1) % cs->nst; moved = 1; }
    if (moved) {
        sfx(SFX_MOVE);
        suspect_expr = EX_NEUTRAL;
        /* after a miss, Shio notices when you pass the statement that does not hold up */
        if (mistakes && !odd_said && cs->st[st_sel].contra >= 0) {
            odd_said = 1;
            shio(SIT_D_ODD, EX_SURPRISED);
        }
    }
    if (any_pressed(BTN_A)) { go(ST_PRESENT); sfx(SFX_POP); }
    else if (any_pressed(BTN_Y)) { notebook = 1; sfx(SFX_POP); }
}

static void update_present(void)
{
    if (any_repeat(BTN_DOWN)) { ev_sel = (ev_sel + 1) % cs->nclues; sfx(SFX_MOVE); }
    if (any_repeat(BTN_UP)) { ev_sel = (ev_sel + cs->nclues - 1) % cs->nclues; sfx(SFX_MOVE); }
    if (any_pressed(BTN_A)) { sfx(SFX_WHOOSH); present(); }
    else if (any_pressed(BTN_B)) { sfx(SFX_BACK); go(ST_TESTIMONY); }
}

static void det_update(void)
{
    state_t++;
    if (shake) shake--;
    if (toast_t) toast_t--;
    if (fade_t) {                                  /* local fade between phases of a case */
        if (++fade_t == 12) {
            go(fade_to);
            fade_to = -1;
        }
        if (fade_t >= 24)
            fade_t = 0;
        return;
    }
    if (paused) {
        if (any_pressed(BTN_A | BTN_START)) { paused = 0; sfx(SFX_OK); }
        else if (any_pressed(BTN_B)) { paused = 0; sfx(SFX_BACK); dlg_clear(); after = -1; fade(ST_LIST); }
        return;
    }
    if (state != ST_LIST && any_pressed(BTN_START)) { paused = 1; sfx(SFX_POP); return; }
    dlg_update();
    if (dlg_busy()) {
        idle_t = 0;
        return;
    }
    if (dlg_just_finished()) {                     /* the press that closed the dialogue does nothing else */
        int a = after;
        after = -1;
        if (a >= 0 && !(a == ST_TESTIMONY && state == ST_TESTIMONY))
            fade(a);
        return;
    }
    if (any_pressed(0xFFF))
        idle_t = 0;
    else if (++idle_t == 60 * 20 && (state == ST_SEARCH || state == ST_TESTIMONY)) {
        shio(SIT_D_IDLE, EX_TIRED);
        idle_t = -60 * 20;
    }
    switch (state) {
    case ST_LIST: update_list(); break;
    case ST_SEARCH: update_search(); break;
    case ST_TESTIMONY: update_testimony(); break;
    case ST_PRESENT: update_present(); break;
    case ST_REACT: if (state_t >= 40) judge(); break;
    case ST_GOTCHA: if (state_t >= 80) solved(); break;
    case ST_RESULT: if (any_pressed(BTN_A | BTN_B)) { sfx(SFX_OK); fade(ST_LIST); } break;
    default: break;
    }
}

/* ---------------------------------------------------------------- draw */
static void draw_list(void)
{
    gradient_v(0, 0, SCREEN_W, SCREEN_H, RGB(34, 62, 72), RGB(18, 26, 44));
    for (int i = 0; i < 40; i++) {                 /* drifting dust in the lamp light */
        u32 h = (u32)i * 2654435761u;
        int x = (int)((h + frame_count / 3) % SCREEN_W), y = (int)((h >> 9) % SCREEN_H);
        pset(x, y, blend(RGB(34, 62, 72), C_MINT, 80));
    }
    circle_fill(250, 110, 70, RGB(40, 76, 84));
    circle_fill(250, 110, 52, RGB(46, 88, 94));
    draw_bust(CH_SHIO, list_sel % 2 ? EX_SMUG : EX_NEUTRAL, 186, 44, 2);
    text_big(10, 8, "WHISKER", 2, C_MINT, C_INK);
    text_big(10, 24, "DETECTIVE", 2, C_WHITE, C_INK);
    draw_magnifier(150, 22);
    for (int i = 0; i < case_count; i++) {
        int y = 50 + i * 22;
        int done = i < 16 ? save.case_done[i] : 0;
        if (i == list_sel) {
            round_box(8, y - 3, 176, 20, C_MINT, RGB(40, 70, 84));
            spr(SPR_PAW, 12, y + 2, 0);
        }
        text_sh(26, y, cases[i].title, i == list_sel ? C_WHITE : C_GREY, C_INK);
        if (done) {
            for (int p = 0; p < 3; p++)
                text(26 + p * 8, y + 9, "\x04", p < done ? C_GOLD : C_DIM);
            text(52, y + 9, "solved", C_MINT);
        } else {
            text(26, y + 9, "new case", i == list_sel ? C_PINK : C_DIM);
        }
    }
    text(8, 168, "^3A^0 open case   ^3B^0 back to Nia", C_GREY);
}

static void draw_testimony(void)
{
    gradient_v(0, 14, SCREEN_W, 166, RGB(74, 58, 108), RGB(30, 24, 52));
    for (int r = 5; r > 0; r--)
        circle_fill(248, 100, 16 + r * 12, blend(RGB(74, 58, 108), RGB(150, 118, 186), (6 - r) * 30));
    rect(0, 156, SCREEN_W, 24, RGB(40, 32, 62));
    hline(0, 156, SCREEN_W, RGB(96, 80, 130));
    /* who is on screen: whoever is talking, else the witness */
    const Statement *s = &cs->st[st_sel];
    int who = s->who, expr = suspect_expr;
    if (dlg_busy() && dlg_who() >= 0) { who = dlg_who(); expr = dlg_expr(); }
    if (state == ST_GOTCHA || state == ST_REACT) { who = s->who; expr = react_ok ? EX_SURPRISED : EX_SMUG; }
    int sx = shake ? isin((int)frame_count * 64) / 64 : 0;
    draw_portrait(184 + sx, 28, 2, who, expr);
    /* suspect tabs */
    for (int i = 0, x = 8; i < 3; i++) {
        int c = suspects[i], on = c == s->who;
        int w = text_w(cast[c].name) + 12;
        round_box(x, 18, w, 12, on ? C_WHITE : C_DIM, on ? cast[c].name_color : C_PANEL);
        text(x + 6, 20, cast[c].name, on ? C_INK : C_GREY);
        x += w + 4;
    }
    /* the statement */
    round_box(8, 34, 170, 82, C_WHITE, RGB(36, 30, 58));
    text(16, 40, "\"", cast[s->who].name_color);
    draw_wrapped(22, 42, 150, s->text, C_WHITE, 6);
    char nav[16], *p = nav;
    p = str_cpy(p, "< ");
    p = str_int(p, st_sel + 1);
    p = str_cpy(p, "/");
    p = str_int(p, cs->nst);
    str_cpy(p, " >");
    text(16, 104, nav, C_GOLD);
    text(80, 104, "testimony", C_DIM);
    if (state == ST_TESTIMONY && !dlg_busy()) {
        text(8, 122, "^3A^0 present evidence", C_GREY);
        text(8, 132, "^3Y^0 notebook  ^3<>^0 statements", C_GREY);
    }
}

static void draw_result(void)
{
    draw_testimony();
    rect_blend(0, 14, SCREEN_W, SCREEN_H - 14, C_NIGHT, 170);
    int t = imin(state_t, 20);
    int sc = 4 - t / 8;
    const char *stamp = "CASE CLOSED";
    int w = text_big_w(stamp, sc);
    round_box(SCREEN_W / 2 - w / 2 - 8, 40 - sc * 2, w + 16, 12 + sc * 7, C_RED, blend(C_NIGHT, C_RED, 40));
    text_big(SCREEN_W / 2 - w / 2, 44 - sc * 2 + 2, stamp, sc, C_RED, C_INK);
    if (state_t > 24) {
        static const char *const verdict[4] = { "", "Got there in the end.", "Sharp eyes.", "Purr-fect deduction!" };
        draw_paws(SCREEN_W / 2 - 19, 88);
        text_center(SCREEN_W / 2, 104, verdict[paws], C_GOLD, C_INK);
        char buf[32], *p = buf;
        p = str_cpy(p, "Cases solved: ");
        p = str_int(p, save.cases_solved);
        p = str_cpy(p, "/");
        str_int(p, case_count);
        text_center(SCREEN_W / 2, 118, buf, C_WHITE, C_INK);
        text_center(SCREEN_W / 2, 150, "^3A^0 back to the case list", C_GREY, C_INK);
    }
}

static void det_draw(void)
{
    if (state == ST_LIST) {
        draw_list();
    } else if (state == ST_BRIEF || state == ST_SEARCH) {
        draw_kitchen(cs->place == PLACE_CAFE);
        if (state == ST_SEARCH) {
            draw_markers();
            if (!dlg_busy()) {
                draw_magnifier(cur_x + 10, cur_y - 10);
                const char *label = found[sel_clue] ? cs->clues[sel_clue].name : "something catches your eye";
                int w = text_w(label) + 10;
                int lx = iclamp(cur_x - w / 2, 2, SCREEN_W - w - 2);
                round_box(lx, 164, w, 13, C_INK, RGB(36, 30, 58));
                text(lx + 5, 166, label, found[sel_clue] ? C_MINT : C_WHITE);
            }
        }
        draw_header("CASE");
        char buf[16], *p = buf;
        p = str_int(p, nfound);
        p = str_cpy(p, "/");
        str_int(p, cs->nclues);
        text(SCREEN_W - 70, 3, buf, C_GOLD);
        if (dlg_busy() && dlg_who() >= 0)
            draw_portrait(250, 62, 1, dlg_who(), dlg_expr());
        if (toast_t) {                             /* new evidence slides in under the header */
            char b[48];
            str_cpy(str_cpy(b, "\x04 Notebook: "), cs->clues[toast_clue].name);
            int w = text_w(b) + 12, y = 16 - imax(0, toast_t - 138);
            round_box(SCREEN_W / 2 - w / 2, y, w, 13, C_MINT, RGB(30, 40, 56));
            text(SCREEN_W / 2 - w / 2 + 6, y + 2, b, C_MINT);
        }
        dlg_draw(4, 128, 312, 48);
        if (notebook)
            draw_evidence("NOTEBOOK", 0);
    } else if (state == ST_RESULT) {
        draw_result();
        draw_header("CASE");
    } else {
        draw_testimony();
        draw_header("CASE");
        if (state == ST_PRESENT)
            draw_evidence("PRESENT EVIDENCE", 1);
        if (state == ST_REACT) {                   /* you slam the clue down */
            int t = state_t;
            rect_blend(0, 14, SCREEN_W, SCREEN_H - 14, C_WHITE, imax(0, 120 - t * 12));
            round_box(20, 60, 160, 40, C_PINK, C_WHITE);
            text_big(34, 70, "Look!", 3, C_ROSE, C_INK);
            text(104, 84, cs->clues[ev_sel].name, C_INK);
        }
        if (state == ST_GOTCHA) {
            int t = state_t;
            rect_blend(0, 0, SCREEN_W, SCREEN_H, C_WHITE, imax(0, 200 - t * 20));
            rect_blend(0, 44, SCREEN_W, 76, C_NIGHT, imin(170, t * 12));
            int sc = t < 8 ? 6 - t / 3 : 4;
            int w = text_big_w("GOTCHA!", sc);
            int jx = t < 30 ? (int)(rnd() % 3) - 1 : 0;
            text_big(SCREEN_W / 2 - w / 2 + jx, 70 - sc * 3, "GOTCHA!", sc, C_PINK, C_INK);
            if (t > 20)
                text_center(SCREEN_W / 2, 110, "the contradiction holds", C_WHITE, C_INK);
        }
        if (notebook)
            draw_evidence("NOTEBOOK", 0);
        dlg_draw(4, 128, 312, 48);
    }
    if (paused) {
        rect_blend(0, 0, SCREEN_W, SCREEN_H, C_NIGHT, 160);
        round_box(90, 64, 140, 48, C_MINT, C_PANEL);
        text_center(SCREEN_W / 2, 72, "Paused", C_MINT, C_INK);
        text_center(SCREEN_W / 2, 88, "^3A^0 resume   ^3B^0 leave case", C_WHITE, C_INK);
    }
    if (fade_t) {
        int a = fade_t <= 12 ? fade_t * 256 / 12 : (24 - fade_t) * 256 / 12;
        rect_blend(0, 0, SCREEN_W, SCREEN_H, C_NIGHT, a);
    }
}

const Scene scene_detective = { det_enter, det_update, det_draw, "detective" };
