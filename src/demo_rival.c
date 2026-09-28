/* Build 1 - Rival!: paw hockey against Mako. Her AI logs which lane your shots arrive in (this match and
 * for good in the save) and shifts her defence toward your habit; her trash talk is written live by
 * NekoLM for each event. Two-player mode: P1 vs P2 with Mako commentating. */
#include "engine.h"
#include "save.h"
#include "cast.h"
#include "nekolm.h"
#include "lines.h"
#include "hub.h"
#include "live.h"

/* table geometry (pixels) */
#define TX0 6
#define TY0 36
#define TX1 244
#define TY1 176
#define TMIDX ((TX0 + TX1) / 2)
#define TMIDY ((TY0 + TY1) / 2)
#define GOAL_H 54
#define PUCK_MAX (85 * FP / 10)   /* px per frame */
#define PR 9            /* paddle radius */
#define BR 5            /* puck radius */
#define FP 256          /* fixed-point one */
#define WIN_SCORE 7

typedef struct { i32 x, y, vx, vy; } Body;

enum { ST_TITLE, ST_SERVE, ST_PLAY, ST_GOAL, ST_OVER };
static int state, state_t, mode_2p, title_sel, hard = 1;   /* hard: rival difficulty rather than chill */
static Body you, her, puck;
static int score[2], streak_owner, streak_len;
static int shake, flash_side, trail_x[6], trail_y[6];
static int lanes[4];            /* this match: top, middle, bottom, off the walls */
static int last_hitter;         /* 0 you, 1 Mako/P2, -1 none */
static int wall_bounced, lane_logged, still_t;
static int strike_t, approach_t, bank_shot, aim_err;
static int hits[2], dash_frames, match_frames;   /* for the playtest log */
static int shots_total[4];  /* Mako's attack: strike frames, lining up, off a wall?, aim error */
static int ai_bias;             /* Mako's learned resting y offset */
static int her_expr, her_expr_t, pattern_called;
static int saves_in_row;

/* ---------------------------------------------------------------- live dialogue */
static int talk_sit, talk_live, talk_hold;
static char talk_text[128];
static const char *const lane_names[4] = { "top", "middle", "bottom", "off the walls" };
static int pattern_lane;
static LiveJob talk_job;

static void say(int sit, int expr)
{
    if (talk_live && sit != SIT_R_WIN && sit != SIT_R_LOSE && talk_hold > 60)
        return;             /* do not cut off a fresh line for small events */
    talk_sit = sit;
    talk_live = 1;
    talk_hold = 0;
    talk_text[0] = 0;
    live_init(&talk_job, SPK_MAKO, sit, sit == SIT_R_PATTERN ? "dir" : 0, lane_names[pattern_lane]);
    her_expr = expr;
    her_expr_t = 150;
}

static void talk_update(void)
{
    talk_hold++;
    if (!talk_live)
        return;
    if (!live_step(&talk_job, 0, 64))
        return;
    talk_live = 0;
    str_cpy(talk_text, talk_job.out);
}

/* ---------------------------------------------------------------- physics */
static void reset_positions(int toward)
{
    you = (Body){ (TX0 + 34) * FP, TMIDY * FP, 0, 0 };
    her = (Body){ (TX1 - 34) * FP, TMIDY * FP, 0, 0 };
    puck = (Body){ TMIDX * FP, TMIDY * FP, 0, 0 };
    strike_t = approach_t = still_t = 0;
    puck.vx = (toward == 0 ? -1 : 1) * 90;
    puck.vy = (rnd_range(0, 1) ? 1 : -1) * rnd_range(20, 70);
    last_hitter = -1;
    wall_bounced = 0;
    lane_logged = 1;
}

/* Paddle speeds in pixels per frame (Q8). The D-pad sets a target velocity; the paddle closes half the gap
 * each frame, so it is at full speed in about 3 frames and stops in about 3: direct, with a little weight. */
#define SPEED_PLAYER (32 * FP / 10)
#define SPEED_BOOST (44 * FP / 10)

static void move_paddle(Body *p, int left_side, int ax, int ay, int top)
{
    if (ax && ay) {                                  /* diagonals are not faster than straight lines */
        ax = ax * 181 / 256;
        ay = ay * 181 / 256;
    }
    i32 tvx = ax * top / FP, tvy = ay * top / FP;
    p->vx += (tvx - p->vx) / 2;
    p->vy += (tvy - p->vy) / 2;
    if (iabs(p->vx) < 8 && !tvx) p->vx = 0;
    if (iabs(p->vy) < 8 && !tvy) p->vy = 0;
    p->x += p->vx;
    p->y += p->vy;
    i32 minx = (left_side ? TX0 + PR : TMIDX + PR) * FP, maxx = (left_side ? TMIDX - PR : TX1 - PR) * FP;
    i32 miny = (TY0 + PR) * FP, maxy = (TY1 - PR) * FP;
    if (p->x < minx || p->x > maxx) { p->x = iclamp(p->x, minx, maxx); p->vx = 0; }   /* no stored push into a wall */
    if (p->y < miny || p->y > maxy) { p->y = iclamp(p->y, miny, maxy); p->vy = 0; }
}

static void collide(Body *p, int who)
{
    i32 dx = puck.x - p->x, dy = puck.y - p->y;
    i32 dist2 = (dx >> 4) * (dx >> 4) + (dy >> 4) * (dy >> 4);          /* in (px*16)^2 */
    i32 rsum = (PR + BR) * 16;
    if (dist2 >= rsum * rsum || dist2 == 0)
        return;
    i32 dist = (i32)isqrt((u32)dist2);                                 /* px*16 */
    /* push the puck out along the normal */
    i32 nx = (dx >> 4) * FP / dist, ny = (dy >> 4) * FP / dist;         /* unit normal, Q8 */
    puck.x = p->x + nx * (PR + BR + 1);
    puck.y = p->y + ny * (PR + BR + 1);
    /* reflect the relative velocity, then add the paddle's own velocity */
    i32 rvx = puck.vx - p->vx, rvy = puck.vy - p->vy;
    i32 dot = (rvx * nx + rvy * ny) / FP;
    if (dot < 0) {
        puck.vx -= 2 * dot * nx / FP;
        puck.vy -= 2 * dot * ny / FP;
    }
    puck.vx += p->vx + nx * 220 / FP;           /* the paddle's own speed, plus a firm push off its face */
    puck.vy += p->vy + ny * 220 / FP;
    sfx(SFX_HIT);
    hits[who]++;
    if (last_hitter != who) {
        last_hitter = who;
        wall_bounced = 0;
        lane_logged = 0;
    }
}

static void log_lane(int y_px)
{
    int lane = wall_bounced ? 3 : (y_px < TY0 + (TY1 - TY0) / 3 ? 0 : y_px > TY1 - (TY1 - TY0) / 3 ? 2 : 1);
    lanes[lane]++;
    shots_total[lane]++;
    if (save.rival_shots[lane < 3 ? lane : 1] < 60000)
        save.rival_shots[lane < 3 ? lane : 1]++;
    /* Mako's defence drifts toward the lane you favour, weighting this match over the lifetime */
    int top = lanes[0] * 3 + save.rival_shots[0] / 8, bot = lanes[2] * 3 + save.rival_shots[2] / 8;
    int mid = lanes[1] * 3 + save.rival_shots[1] / 8 + 1;
    ai_bias = iclamp((bot - top) * 28 / (top + bot + mid), -22, 22);
    int total = lanes[0] + lanes[1] + lanes[2] + lanes[3];
    if (!mode_2p && total >= 5 && !pattern_called) {
        for (int k = 0; k < 4; k++)
            if (lanes[k] * 100 / total >= 50) {
                pattern_lane = k;
                pattern_called = 1;
                say(SIT_R_PATTERN, EX_SMUG);
                break;
            }
    }
}

static void step_puck(void)
{
    /* sub-steps keep fast shots from tunnelling through paddles */
    for (int s = 0; s < 3; s++) {
        puck.x += puck.vx / 3;
        puck.y += puck.vy / 3;
        if (puck.y < (TY0 + BR) * FP) { puck.y = (TY0 + BR) * FP; puck.vy = -puck.vy * 9 / 10; wall_bounced = 1; sfx(SFX_WALL); }
        if (puck.y > (TY1 - BR) * FP) { puck.y = (TY1 - BR) * FP; puck.vy = -puck.vy * 9 / 10; wall_bounced = 1; sfx(SFX_WALL); }
        int in_goal = iabs(puck.y / FP - TMIDY) < GOAL_H / 2;
        if (puck.x < (TX0 + BR) * FP && !in_goal) { puck.x = (TX0 + BR) * FP; puck.vx = -puck.vx * 9 / 10; sfx(SFX_WALL); }
        if (puck.x > (TX1 - BR) * FP && !in_goal) { puck.x = (TX1 - BR) * FP; puck.vx = -puck.vx * 9 / 10; sfx(SFX_WALL); }
        collide(&you, 0);
        collide(&her, 1);
    }
    /* friction and a speed cap */
    puck.vx = puck.vx * 254 / 256;
    puck.vy = puck.vy * 254 / 256;
    /* the table's air: a puck left still for two seconds drifts back toward the middle */
    if (iabs(puck.vx) + iabs(puck.vy) < FP / 4) {
        if (++still_t > 120) {
            i32 dx = TMIDX * FP - puck.x, dy = TMIDY * FP - puck.y;
            i32 d = (i32)isqrt((u32)((dx >> 4) * (dx >> 4) + (dy >> 4) * (dy >> 4))) * 16 + 1;
            puck.vx += dx * FP / d;
            puck.vy += dy * FP / d;
            still_t = 0;
            sfx(SFX_WHOOSH);
        }
    } else {
        still_t = 0;
    }
    i32 sp = (i32)isqrt((u32)((puck.vx >> 2) * (puck.vx >> 2) + (puck.vy >> 2) * (puck.vy >> 2))) * 4;
    if (sp > PUCK_MAX) { puck.vx = puck.vx * PUCK_MAX / sp; puck.vy = puck.vy * PUCK_MAX / sp; }
    /* your shots are logged by the lane they arrive in on Mako's half */
    if (last_hitter == 0 && !lane_logged && puck.x > (TMIDX + 20) * FP) {
        lane_logged = 1;
        log_lane(puck.y / FP);
    }
    for (int k = 5; k > 0; k--) { trail_x[k] = trail_x[k - 1]; trail_y[k] = trail_y[k - 1]; }
    trail_x[0] = puck.x / FP; trail_y[0] = puck.y / FP;
}

/* ---------------------------------------------------------------- Mako's paddle */
static void ai_move(void)
{
    int lead = score[1] - score[0];
    int speed = (hard ? 19 : 15) * FP / 10 + (lead < -1 ? 4 * FP / 10 : 0) - (lead > 1 ? 4 * FP / 10 : 0);   /* rubber band */
    int tx, ty, top = speed;
    int px = puck.x / FP, py = puck.y / FP, hx = her.x / FP, hy = her.y / FP;
    if (puck.x > TMIDX * FP && puck.vx < 2 * FP) {
        /* attack: line up behind the puck on the line from your goal (aiming at the side your paddle is not
         * covering), then strike through it; strike anyway if the spot behind it cannot be reached */
        int gy = you.y / FP < TMIDY ? TMIDY + GOAL_H / 3 : TMIDY - GOAL_H / 3;
        if (approach_t == 0 && strike_t == 0)
            bank_shot = hard && rnd_range(0, 3) == 0;   /* now and then, off the wall on your open side */
        if (approach_t == 0 && strike_t == 0)
            aim_err = hard ? rnd_range(-14, 14) : rnd_range(-22, 22);   /* good, not perfect */
        gy += aim_err;
        if (bank_shot)
            gy = gy > TMIDY ? 2 * TY1 - gy : 2 * TY0 - gy;
        int dx = px - TX0, dy = py - gy;
        int d = imax(1, (int)isqrt((u32)(dx * dx + dy * dy)));
        int setx = iclamp(px + dx * (PR + BR + 5) / d, TMIDX + PR, TX1 - PR);
        int sety = iclamp(py + dy * (PR + BR + 5) / d, TY0 + PR, TY1 - PR);
        if (strike_t > 0) {
            strike_t--;
            tx = px - dx * 24 / d;
            ty = py - dy * 24 / d;
            top = speed * 9 / 8;
        } else if (hx < px + 3) {                   /* in front of the puck: go around it, not through it */
            tx = px + 16;
            ty = py + (hy < py ? -22 : 22);
            approach_t++;
        } else {
            tx = setx;
            ty = sety;
            approach_t++;
            if (iabs(hx - setx) + iabs(hy - sety) < 5)
                approach_t = 999;
        }
        if (approach_t > 50) {                      /* lined up, or it cannot be done: strike */
            strike_t = 22;
            approach_t = 0;
        }
    } else {
        strike_t = 0;
        approach_t = 0;
        /* defend: stay between the puck and the goal, leaning toward your usual lane */
        tx = TX1 - 30;
        int predict = puck.y / FP;
        if (puck.vx > 0) {
            int frames = ((TX1 - 30) * FP - puck.x) / (puck.vx ? puck.vx : 1);
            predict = iclamp(puck.y / FP + puck.vy * frames / FP / 2, TY0 + PR, TY1 - PR);
        }
        ty = (predict * 3 + (TMIDY + ai_bias)) / 4;
    }
    /* full speed when far from the target, slowing within 8 px so she settles instead of jittering */
    int ax = iclamp((tx * FP - her.x) / 8, -FP, FP);
    int ay = iclamp((ty * FP - her.y) / 8, -FP, FP);
    move_paddle(&her, 0, ax, ay, top);
}

/* ---------------------------------------------------------------- scene */
static void rival_enter(void)
{
    state = ST_TITLE;
    state_t = 0;
    title_sel = 0;
    talk_live = 0;
    talk_text[0] = 0;
    music(MUS_HOCKEY);
}

static void start_match(void)
{
    score[0] = score[1] = 0;
    for (int k = 0; k < 4; k++) lanes[k] = 0;
    pattern_called = 0;
    streak_len = 0;
    saves_in_row = 0;
    ai_bias = 0;
    hits[0] = hits[1] = dash_frames = match_frames = 0;
    for (int k = 0; k < 4; k++) shots_total[k] = 0;
    reset_positions(rnd_range(0, 1));
    state = ST_SERVE;
    state_t = 0;
    say(mode_2p ? SIT_R_COMMENTARY : SIT_R_START, EX_SMUG);
}

static void goal(int scorer)
{
    score[scorer]++;
    shake = 10;
    flash_side = scorer ? 1 : 2;
    sfx(scorer == 0 ? SFX_GOAL : SFX_LOSE);
    if (streak_owner == scorer) streak_len++; else { streak_owner = scorer; streak_len = 1; }
    state = ST_GOAL;
    state_t = 0;
    if (score[scorer] >= WIN_SCORE) {
        state = ST_OVER;
        char *b = log_buf(), *q = b;               /* the match, for the playtest log */
        q = str_cpy(q, "rival: match ");
        q = str_int(q, score[0]);
        q = str_cpy(q, "-");
        q = str_int(q, score[1]);
        q = str_cpy(q, mode_2p ? " (P1-P2, 2 players)" : hard ? " (you-Mako, rival)" : " (you-Mako, chill)");
        q = str_cpy(q, " in ");
        q = str_int(q, match_frames / 60);
        q = str_cpy(q, " s; paddle hits ");
        q = str_int(q, hits[0]);
        q = str_cpy(q, "-");
        q = str_int(q, hits[1]);
        q = str_cpy(q, "; your dash ");
        q = str_int(q, dash_frames * 100 / imax(1, match_frames));
        q = str_cpy(q, "% of the time; your shots top/mid/low/wall ");
        for (int k = 0; k < 4; k++) { q = str_int(q, shots_total[k]); if (k < 3) q = str_cpy(q, "/"); }
        platform_log(b);
        if (!mode_2p) {
            if (scorer == 0) { save.rival_wins++; say(SIT_R_LOSE, EX_FLUSTERED); }
            else { save.rival_losses++; say(SIT_R_WIN, EX_HAPPY); }
        } else {
            say(SIT_R_COMMENTARY, EX_SPARKLE);
        }
        return;
    }
    if (mode_2p) { if (streak_len >= 2 || rnd_range(0, 2) == 0) say(SIT_R_COMMENTARY, EX_SMUG); return; }
    if (streak_len == 3) say(scorer == 0 ? SIT_R_YOUR_STREAK : SIT_R_HER_STREAK, scorer == 0 ? EX_ANNOYED : EX_SMUG);
    else if (score[0] == WIN_SCORE - 1 && scorer == 0) say(SIT_R_MATCH_POINT_YOU, EX_SURPRISED);
    else if (score[1] == WIN_SCORE - 1 && scorer == 1) say(SIT_R_MATCH_POINT_HER, EX_SMUG);
    else say(scorer == 0 ? SIT_R_YOU_SCORE : SIT_R_SHE_SCORES, scorer == 0 ? EX_ANNOYED : EX_HAPPY);
}

static void rival_update(void)
{
    state_t++;
    talk_update();
    if (her_expr_t > 0 && --her_expr_t == 0)
        her_expr = EX_NEUTRAL;
    if (shake) shake--;
    if (state == ST_TITLE) {
        if (any_repeat(BTN_DOWN)) { title_sel = (title_sel + 1) % 3; sfx(SFX_MOVE); }
        if (any_repeat(BTN_UP)) { title_sel = (title_sel + 2) % 3; sfx(SFX_MOVE); }
        if (any_pressed(BTN_A)) { sfx(SFX_OK); mode_2p = title_sel == 2; hard = title_sel == 1; start_match(); }
        if (any_pressed(BTN_B)) { sfx(SFX_BACK); scene_fade_to(&scene_menu); }
        return;
    }
    if (any_pressed(BTN_START)) { sfx(SFX_BACK); scene_fade_to(&scene_menu); return; }
    /* your paddle (and player 2's in two-player mode) */
    int ax = (btn_held(0, BTN_RIGHT) ? FP : 0) - (btn_held(0, BTN_LEFT) ? FP : 0);
    int ay = (btn_held(0, BTN_DOWN) ? FP : 0) - (btn_held(0, BTN_UP) ? FP : 0);
    move_paddle(&you, 1, ax, ay, btn_held(0, BTN_A | BTN_B) ? SPEED_BOOST : SPEED_PLAYER);
    if (state == ST_PLAY) {
        match_frames++;
        dash_frames += btn_held(0, BTN_A | BTN_B) != 0;
    }
    if (mode_2p) {
        int bx = (btn_held(1, BTN_RIGHT) ? FP : 0) - (btn_held(1, BTN_LEFT) ? FP : 0);
        int by = (btn_held(1, BTN_DOWN) ? FP : 0) - (btn_held(1, BTN_UP) ? FP : 0);
        move_paddle(&her, 0, bx, by, btn_held(1, BTN_A | BTN_B) ? SPEED_BOOST : SPEED_PLAYER);
    }
    if (state == ST_SERVE) {
        if (state_t > 50) state = ST_PLAY;
        return;
    }
    if (state == ST_GOAL) {
        if (state_t > 70) { reset_positions(flash_side == 1 ? 0 : 1); state = ST_SERVE; state_t = 0; }
        return;
    }
    if (state == ST_OVER) {
        if (state_t > 60 && any_pressed(BTN_A)) { sfx(SFX_OK); start_match(); }
        if (state_t > 60 && any_pressed(BTN_B)) {
            sfx(SFX_BACK);
            if (!mode_2p)
                hub_report(score[0] > score[1] ? SIT_AFTER_WIN_RIVAL : SIT_AFTER_LOSE_RIVAL);
            scene_fade_to(&scene_menu);
        }
        return;
    }
    if (!mode_2p)
        ai_move();
    int was_toward_her = puck.vx > 0 && puck.x > (TMIDX + 30) * FP;
    step_puck();
    /* a save: your shot was heading for her goal and she turned it around */
    if (!mode_2p && was_toward_her && puck.vx < 0 && last_hitter == 1 && iabs(puck.vx) > 3 * FP) {
        if (++saves_in_row >= 2) { say(SIT_R_SAVE, EX_SMUG); saves_in_row = 0; }
    }
    if (puck.x < (TX0 - BR) * FP) goal(1);
    else if (puck.x > (TX1 + BR) * FP) goal(0);
}

/* ---------------------------------------------------------------- drawing */
static void draw_paw(int cx, int cy, u16 fur, u16 dark, u16 bean)
{
    circle_fill(cx, cy + 1, PR, dark);
    circle_fill(cx, cy, PR, fur);
    circle_fill(cx, cy + 2, 4, bean);
    circle_fill(cx - 5, cy - 4, 2, bean);
    circle_fill(cx, cy - 6, 2, bean);
    circle_fill(cx + 5, cy - 4, 2, bean);
    circle_line(cx, cy, PR, C_INK);
}

static void draw_table(int ox, int oy)
{
    rect(ox + TX0 - 4, oy + TY0 - 4, TX1 - TX0 + 8, TY1 - TY0 + 8, RGB(255, 122, 168));
    rect(ox + TX0 - 2, oy + TY0 - 2, TX1 - TX0 + 4, TY1 - TY0 + 4, RGB(217, 70, 126));
    gradient_v(ox + TX0, oy + TY0, TX1 - TX0, TY1 - TY0, RGB(222, 240, 255), RGB(196, 222, 250));
    for (int y = TY0 + 6; y < TY1; y += 12)                    /* subtle ice lines */
        hline(ox + TX0 + 2, oy + y, TX1 - TX0 - 4, RGB(210, 232, 252));
    vline(ox + TMIDX, oy + TY0, TY1 - TY0, RGB(255, 170, 200));
    circle_line(ox + TMIDX, oy + TMIDY, 18, RGB(255, 170, 200));
    circle_line(ox + TX0, oy + TMIDY, 26, RGB(160, 196, 240));
    circle_line(ox + TX1, oy + TMIDY, 26, RGB(160, 196, 240));
    /* goals */
    rect(ox + TX0 - 4, oy + TMIDY - GOAL_H / 2, 4, GOAL_H, C_INK);
    rect(ox + TX1, oy + TMIDY - GOAL_H / 2, 4, GOAL_H, C_INK);
    /* paw decals */
    spr(SPR_PAW, ox + TMIDX - 5, oy + TMIDY - 5, 0);
}

static void rival_draw(void)
{
    int ox = shake ? rnd_range(-2, 2) : 0, oy = shake ? rnd_range(-2, 2) : 0;
    gradient_v(0, 0, SCREEN_W, SCREEN_H, RGB(58, 40, 82), RGB(30, 24, 52));
    draw_table(ox, oy);
    if (state != ST_TITLE) {
        int sp = iabs(puck.vx) + iabs(puck.vy);
        if (sp > 4 * FP)
            for (int k = 5; k > 0; k--)
                circle_fill(ox + trail_x[k], oy + trail_y[k], BR - k / 2, blend(RGB(222, 240, 255), RGB(255, 150, 190), 150 - k * 20));
        draw_paw(ox + you.x / FP, oy + you.y / FP, RGB(170, 200, 255), RGB(90, 120, 200), RGB(255, 170, 200));
        draw_paw(ox + her.x / FP, oy + her.y / FP, RGB(255, 138, 216), RGB(142, 32, 104), RGB(255, 220, 240));
        int px = ox + puck.x / FP, py = oy + puck.y / FP;
        circle_fill(px, py + 1, BR, RGB(160, 60, 100));
        circle_fill(px, py, BR, RGB(255, 150, 120));
        line(px - 3, py - 1, px + 2, py - 3, RGB(255, 220, 190));
        line(px - 3, py + 2, px + 3, py, RGB(220, 100, 90));
        circle_line(px, py, BR, C_INK);
    }
    if (flash_side && state == ST_GOAL && state_t < 16)
        rect_blend(flash_side == 2 ? TMIDX : TX0, TY0, TMIDX - TX0, TY1 - TY0, C_WHITE, 120 - state_t * 7);
    /* right panel: Mako, score, her notes */
    rect(248, 0, 72, SCREEN_H, RGB(36, 28, 58));
    draw_bust(CH_MAKO, her_expr, 252, 0, 1);
    char buf[48];
    char *p = str_int(buf, score[0]);
    p = str_cpy(p, " : ");
    str_int(p, score[1]);
    text_center(284, 68, buf, C_WHITE, C_INK);
    text_center(284, 78, mode_2p ? "P1   P2" : "you  Mako", C_DIM, C_INK);
    if (!mode_2p) {
        text_sh(252, 94, "Mako's notes", C_PINK, C_INK);
        int total = lanes[0] + lanes[1] + lanes[2] + lanes[3];
        static const char *const lab[4] = { "top", "mid", "low", "wall" };
        for (int k = 0; k < 4; k++) {
            int y = 106 + k * 12;
            text(252, y, lab[k], C_GREY);
            int w = total ? lanes[k] * 36 / total : 0;
            rect(276, y + 1, 36, 6, RGB(60, 50, 90));
            rect(276, y + 1, w, 6, k == pattern_lane && pattern_called ? C_PINK : C_VIOLET);
        }
        char wl[32];
        char *q = str_cpy(wl, "W ");
        q = str_int(q, save.rival_wins);
        q = str_cpy(q, "  L ");
        str_int(q, save.rival_losses);
        text_sh(252, 158, wl, C_DIM, C_INK);
    }
    /* Mako's speech bubble (live NekoLM text) */
    if (talk_text[0] || talk_live) {
        round_box(4, 3, 240, 28, C_WHITE, RGB(255, 246, 250));
        if (talk_live) {                               /* NekoLM is writing her next line */
            for (int i = 0; i < 1 + (int)(frame_count / 10) % 3; i++)
                circle_fill(14 + i * 7, 16, 2, C_ROSE);
        } else {
            int starts[3], lens[3];
            int n = wrap_text(talk_text, 228, starts, lens, 2);
            for (int i = 0; i < n; i++)
                text_n(10, 7 + i * 10, talk_text + starts[i], lens[i], C_INK);
        }
        for (int k = 0; k < 4; k++)
            hline(244 - k, 12 + k, 1 + k, RGB(255, 246, 250));
    }
    if (state == ST_TITLE) {
        rect_blend(0, 0, 248, SCREEN_H, RGB(20, 14, 34), 170);
        text_big(26, 44, "RIVAL!", 3, C_PINK, C_INK);
        text_sh(26, 70, "Paw hockey vs Mako. First to 7.", C_WHITE, C_INK);
        text_sh(26, 82, "She learns where you shoot.", C_GOLD, C_INK);
        static const char *const opts[3] = { "vs Mako: chill", "vs Mako: rival", "2 players (Mako commentates)" };
        for (int i = 0; i < 3; i++) {
            int y = 100 + i * 14;
            if (i == title_sel) round_box(22, y - 3, 190, 13, C_PINK, C_PANEL2);
            text_sh(30, y, opts[i], i == title_sel ? C_WHITE : C_GREY, C_INK);
        }
        text_sh(26, 150, "Move: d-pad   A/B: dash   Start: quit", C_DIM, C_INK);
    }
    if (state == ST_SERVE && state_t < 40)
        text_center(TMIDX, TMIDY - 30, state_t < 20 ? "Ready..." : "Go!", C_ROSE, C_WHITE);
    if (state == ST_OVER) {
        rect_blend(TX0, TY0, TX1 - TX0, TY1 - TY0, RGB(20, 14, 34), 150);
        const char *res = mode_2p ? (score[0] > score[1] ? "Player 1 wins!" : "Player 2 wins!")
                                  : (score[0] > score[1] ? "You beat Mako!" : "Mako wins.");
        text_big(TMIDX - text_big_w(res, 2) / 2, TMIDY - 22, res, 2, C_GOLD, C_INK);
        if (state_t > 60)
            text_center(TMIDX, TMIDY + 6, "A: rematch    B: back to Nia's desk", C_WHITE, C_INK);
    }
}

const Scene scene_rival = { rival_enter, rival_update, rival_draw, "rival" };
