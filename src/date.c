/* Build 06: Thin Walls, the dating sim. Fourteen evenings in one apartment building with Nia (your roommate since
 * her flat flooded), Mako (next door, working off a lost bet) and Shio (next door, her water shut off). Each route
 * comes from the character's source card: its hidden stats, its milestones and its progress and regression
 * triggers, with the card's own numbers (tools/date_spec.py). The scenes were written from the cards by a large
 * model (tools/date_gen.py); every line of hers, and every narrated tell, sets her ears, tail, hands and where she
 * stands, and portrait.c animates them. Suggestive at most: a moment that would go further fades to black. The
 * rewards are her milestones, outfits and the endings. */
#include "engine.h"
#include "save.h"
#include "cast.h"
#include "portrait.h"
#include "gen/date_data.h"

enum { ST_TITLE, ST_EVENING, ST_PICK, ST_SCENE, ST_AFTER, ST_WARDROBE, ST_ALBUM };

static int state, sel, t;
static Portrait por, trio[3], mini;
static const DScene *scene;
static int ip, shown_x2, you_line, choosing, nopt, after_choice, opt_idx[4], opt_rows[4], bar_y;
static u8 stat_tmp[4];                               /* the scene's stat changes, kept until the scene is done */
static int cue_step, cue_t;                          /* a narrated line's later tells */
static int knock_n, knock_soft, knock_t, door, door_t, door_bump;
static int flash_t, thunder_t, heart_t, heart_x, fade_t, dim;
static int black, black_dir, music_vol = 150;        /* a fade to black (0..32), and the music under it */
static char notes[3][64];
static int nnotes, new_outfit, ward_who, album_sel;

static const char *const names[3] = { "Nia", "Mako", "Shio" };
static const char *const loc_names[DL_COUNT] = { "your front door", "the kitchen", "the couch", "the hallway",
                                                 "the stairwell", "the 24-hour store", "her place" };

/* ---------------------------------------------------------------- story state */

static int primary(int who) { return save.date.stat[who][date_primary[who]]; }

static const DScene *find_scene(int who, int kind, int beat)
{
    for (int i = 0; i < date_scene_count; i++)
        if (date_scenes[i].who == who && date_scenes[i].kind == kind && date_scenes[i].beat == beat)
            return &date_scenes[i];
    return 0;
}

static int milestones_done(int who)
{
    int n = 0;
    for (int k = 1; k <= 6; k++)
        n += (save.date.milestones[who] >> k) & 1;
    return n;
}

/* Tonight behind her door: her intro; the card's next milestone once her stat has reached it (at most one every
 * other evening, so the route has room to breathe); otherwise an ordinary evening. The last evening is the ending
 * her milestones earned, if her stat still holds it. */
static const DScene *next_scene(int who)
{
    u8 m = save.date.milestones[who];
    int ev = save.date.evening;
    if (ev >= date_evenings) {
        const DScene *best = find_scene(who, DK_ENDING, 0), *friends = find_scene(who, DK_ENDING, 1);
        if ((m & (1 << 6)) && primary(who) >= best->threshold)
            return best;
        if ((m & (1 << 3)) && primary(who) >= friends->threshold)
            return friends;
        return find_scene(who, DK_ENDING, 2);
    }
    if (!(m & 1))
        return find_scene(who, DK_INTRO, 0);
    for (int k = 1; k <= 6; k++)
        if (!(m & (1 << k))) {
            const DScene *s = find_scene(who, DK_MILESTONE, k);
            if (s && primary(who) >= s->threshold && ev >= 2 * k)
                return s;
            break;
        }
    return find_scene(who, DK_HANGOUT, save.date.hangout_next[who] % 6 + 1);
}

static void new_story(void)
{
    save.date.active = 1;
    save.date.evening = 1;
    for (int w = 0; w < 3; w++) {
        for (int k = 0; k < 4; k++)
            save.date.stat[w][k] = date_stat_start[w][k];
        save.date.milestones[w] = 0;
        save.date.hangout_next[w] = 0;
        save.date.visits[w] = 0;
    }
    platform_log("date: new story");
}

/* ---------------------------------------------------------------- scene player */

static void end_scene(void);

static void apply_cues(const DStep *st)
{
    int narr = st->who == 2;
    int face = st->face == DC_KEEP ? -1 : st->face, ears = st->ears == DC_KEEP ? -1 : st->ears;
    int tail = st->tail == DC_KEEP ? -1 : st->tail, pose = st->pose == DC_KEEP ? -1 : st->pose;
    int gest = st->gesture == DC_KEEP ? -1 : st->gesture;
    int blush = st->flags & DF_BLUSH ? 1 : narr ? -1 : 0;
    int aside = st->flags & DF_AWAY ? 1 : st->flags & DF_LOOK ? 0 : narr ? -1 : 0;
    portrait_cue(&por, face, ears, tail, pose, gest, blush, aside);
    if (st->flags & DF_EAR_FLICK)
        portrait_flick_ear(&por);
}

static void step_begin(void)
{
    const DStep *st = &date_steps[ip];
    shown_x2 = 0;
    switch (st->kind) {
    case DS_LINE:
        if (st->who == 3) {                           /* fade to black: the text waits for the dark */
            black_dir = 1;
            cue_step = -1;
            break;
        }
        if (black > 0)
            black_dir = -1;                           /* and back */
        if (st->who == 1) {
            if (rnd() & 1) portrait_flick_ear(&por);  /* she listens */
            break;
        }
        apply_cues(st);
        if (st->who == 2) {
            if (st->arg) { knock_n = st->arg & 15; knock_soft = st->arg >> 4; knock_t = 0; }
            if (st->flags & DF_FLASH) flash_t = 12;
            if (st->flags & DF_THUNDER) thunder_t = st->flags & DF_FLASH ? 24 : 1;
        }
        cue_step = date_steps[ip + 1].kind == DS_CUE ? ip + 1 : -1;
        cue_t = cue_step >= 0 ? date_steps[ip + 1].arg : 0;
        break;
    case DS_CHOICE: {
        int j = ip + 1;
        nopt = 0;
        for (int k = 0; k < st->arg && k < 4; k++) {
            int starts[2], lens[2];
            opt_idx[nopt] = j;
            opt_rows[nopt++] = imax(1, wrap_text(date_steps[j].text, 288, starts, lens, 2));
            while (date_steps[j].kind != DS_OPT_END) j++;
            j++;
        }
        after_choice = j;
        choosing = 1;
        sel = 0;
        bar_y = 0;
        break;
    }
    case DS_OPT_END:
        ip = after_choice;
        step_begin();
        break;
    case DS_END:
        end_scene();
        break;
    default:                                          /* a stray CUE: its line was skipped */
        apply_cues(st);
        ip++;
        step_begin();
        break;
    }
}

static void advance(void)
{
    if (cue_step >= 0) { apply_cues(&date_steps[cue_step]); cue_step = -1; }
    ip++;
    if (date_steps[ip].kind == DS_CUE)
        ip++;
    step_begin();
}

static void start_scene(const DScene *s)
{
    scene = s;
    ip = s->step0;
    choosing = you_line = 0;
    cue_step = -1;
    knock_n = knock_t = 0;
    flash_t = thunder_t = heart_t = 0;
    fade_t = 16;
    black = black_dir = 0;
    audio_music_volume = music_vol;
    for (int k = 0; k < 4; k++)
        stat_tmp[k] = save.date.stat[s->who][k];
    dim = s->weather & DW_DARK ? 170 : s->late ? 80 : 0;
    portrait_init(&por, s->who, save.date.outfit_worn[s->who]);
    int outside = s->loc == DL_DOORWAY || s->loc == DL_HALLWAY;
    portrait_cue(&por, EX_NEUTRAL, EM_UP, TM_SWAY, outside ? PO_FAR : PO_MID, GE_NONE, 0, 0);
    portrait_snap(&por);
    /* at your front door she knocks first, and the door opens when the knocking is done */
    const DStep *first = &date_steps[ip];
    door = s->loc == DL_DOORWAY && first->kind == DS_LINE && first->who == 2 && first->arg ? 0 : 16;
    door_t = 0;
    state = ST_SCENE;
    static const char *const kinds[4] = { " intro", " milestone ", " evening ", " ending " };
    char *lb = log_buf(), *b = lb;
    b = str_int(str_cpy(b, "date: evening "), save.date.evening);
    b = str_cpy(str_cpy(str_cpy(b, ", "), names[s->who]), kinds[s->kind]);
    if (s->kind != DK_INTRO)
        str_int(b, s->beat);
    platform_log(lb);
    step_begin();
}

static void pick_option(int k)
{
    const DStep *op = &date_steps[opt_idx[k]];
    int who = scene->who;
    const DEffect *e = &date_effects[who][op->arg];
    for (int s = 0; s < 4; s++)
        stat_tmp[s] = (u8)iclamp(stat_tmp[s] + e->d[s], 0, 100);
    if (e->d[date_primary[who]] >= 4) {               /* something she will remember */
        heart_t = 80;
        heart_x = (por.x >> 8) + (por.s >> 3) + (int)(rnd() % 20) - 10;
        sfx(SFX_CHIME);
    } else {
        sfx(SFX_OK);
    }
    char *lb = log_buf(), *b = lb;
    b = str_cpy(str_cpy(b, "date: chose "), date_trigger_names[who][op->arg]);
    b = str_cpy(str_cpy(b, ", "), date_stat_names[who][date_primary[who]]);
    str_int(str_cpy(b, " "), stat_tmp[date_primary[who]]);
    platform_log(lb);
    choosing = 0;
    ip = opt_idx[k];                                  /* your words first, then her reaction */
    you_line = 1;
    shown_x2 = 0;
}

static void note(const char *a, const char *b)
{
    if (nnotes < 3)
        str_cpy(str_cpy(notes[nnotes++], a), b);
}

static void end_scene(void)
{
    int who = scene->who;
    nnotes = 0;
    new_outfit = -1;
    flash_t = thunder_t = knock_n = knock_soft = 0;
    cue_step = -1;
    if (black > 0)
        black_dir = -1;                               /* a scene that ended in the dark comes back up for the card */
    int p = date_primary[who], delta = stat_tmp[p] - save.date.stat[who][p];
    note("", delta >= 8 ? "She seemed glad you were there." : delta > 0 ? "A good evening, quietly."
                        : delta < 0 ? "Something went unsaid tonight." : "A quiet evening.");
    for (int k = 0; k < 4; k++)
        save.date.stat[who][k] = stat_tmp[k];
    save.date.visits[who]++;
    save.date.last_pick = (u8)who;
    if (scene->kind == DK_HANGOUT)
        save.date.hangout_next[who]++;
    if (scene->kind == DK_INTRO || scene->kind == DK_MILESTONE) {
        int bit = scene->kind == DK_INTRO ? 0 : scene->beat;
        save.date.milestones[who] |= (u8)(1 << bit);
        if (!(save.date.album[who] & (1 << bit)))
            note("Album: ", scene->kind == DK_INTRO ? "the first evening" : "a new page");
        save.date.album[who] |= (u8)(1 << bit);
    }
    if (scene->outfit && !(save.date.outfit_unlocked[who] & (1 << scene->outfit))) {
        save.date.outfit_unlocked[who] |= (u8)(1 << scene->outfit);
        note("New outfit: ", portrait_outfit_name(who, scene->outfit));
        new_outfit = scene->outfit;
        portrait_init(&mini, who, new_outfit);
        portrait_cue(&mini, EX_HAPPY, EM_PERK, TM_CURL, -1, GE_NONE, 1, 0);
    }
    if (scene->kind == DK_ENDING) {
        static const char *const ends[3] = { "the one you hoped for", "friends, a door left open", "a quiet goodbye" };
        save.date.endings[who] |= (u8)(1 << scene->beat);
        note("Ending: ", ends[scene->beat]);
        save.date.active = 0;
        char *lb = log_buf();
        str_cpy(str_cpy(str_cpy(lb, "date: ending "), names[who]),
                scene->beat == 0 ? " best" : scene->beat == 1 ? " friends" : " drift");
        platform_log(lb);
    } else {
        save.date.evening++;
        save.date.stat[1][0] = (u8)imin(100, save.date.stat[1][0] + 5);   /* the card: Mako's Nice, +5 a day */
    }
    state = ST_AFTER;
    t = 0;
}

/* ---------------------------------------------------------------- backgrounds */

static int weather;

static u16 T(u16 c) { return dim ? blend(c, RGB(10, 8, 30), dim) : c; }

static void disc_blend(int cx, int cy, int r, u16 c, int alpha)
{
    for (int y = -r; y <= r; y++) {
        int yy = cy + y;
        if (yy < 0 || yy >= SCREEN_H)
            continue;
        int w = (int)isqrt((u32)(r * r - y * y)), x0 = imax(0, cx - w), x1 = imin(SCREEN_W - 1, cx + w);
        u16 *p = fb + yy * SCREEN_W;
        for (int x = x0; x <= x1; x++)
            p[x] = blend(p[x], c, alpha);
    }
}

static void glow(int x, int y, int r, u16 c)          /* a soft light over whatever is behind it */
{
    c = T(c);
    disc_blend(x, y, r, c, 40);
    disc_blend(x, y, r * 2 / 3, c, 50);
    disc_blend(x, y, r / 3, c, 80);
}

static void window(int x, int y, int w, int h)
{
    int rain = weather & DW_RAIN;
    u16 top = T(rain ? RGB(40, 46, 70) : RGB(30, 36, 92)), bot = T(rain ? RGB(74, 80, 104) : RGB(112, 84, 150));
    if (flash_t > 0) {
        top = blend(top, C_WHITE, flash_t * 20);
        bot = blend(bot, C_WHITE, flash_t * 20);
    }
    gradient_v(x, y, w, h, top, bot);
    if (!rain) {
        for (int i = 0; i < w * h / 260 + 3; i++) {   /* stars, a few twinkling */
            u32 k = (u32)(i + 7) * 2654435761u;
            int sx = x + 2 + (int)(k % (u32)(w - 4)), sy = y + 2 + (int)((k >> 11) % (u32)(h * 2 / 3));
            int tw = (int)((frame_count / 9 + (k >> 20)) % 50);
            pset(sx, sy, tw < 2 ? C_WHITE : T(C_GREY));
        }
        circle_fill(x + w - 14, y + 12, 6, T(RGB(255, 244, 214)));   /* a crescent moon */
        circle_fill(x + w - 11, y + 10, 5, top);
    } else {
        for (int i = 0; i < w / 3; i++) {             /* rain running down the glass */
            u32 k = (u32)(i + 3) * 2246822519u;
            int rx = x + (int)(k % (u32)w);
            int ry = y + (int)(((k >> 9) + frame_count * (3 + (k >> 28) % 3)) % (u32)h);
            vline(rx, ry, imin(5, y + h - ry), T(RGB(170, 182, 214)));
        }
    }
    u16 wood = T(RGB(120, 88, 66));
    rect_line(x - 2, y - 2, w + 4, h + 4, wood);
    rect_line(x - 1, y - 1, w + 2, h + 2, T(RGB(90, 64, 50)));
    vline(x + w / 2, y, h, wood);
    hline(x, y + h / 2, w, wood);
}

static void room(u16 wall_top, u16 wall_bot, u16 floor)
{
    gradient_v(0, 0, SCREEN_W, 130, T(wall_top), T(wall_bot));
    rect(0, 130, SCREEN_W, 50, T(floor));
    hline(0, 130, SCREEN_W, T(blend(floor, C_WHITE, 40)));
}

static void draw_bg(int loc, int who)
{
    u32 f = frame_count;
    switch (loc) {
    case DL_DOORWAY: {                                /* your entryway, the front door open to the hallway */
        room(RGB(104, 88, 110), RGB(72, 60, 86), RGB(96, 70, 58));
        rect(178, 0, 140, 136, T(RGB(78, 58, 48)));
        int flick = (f / 4) % 173 == 0 ? 40 : 0;      /* the hallway bulb, not quite steady */
        gradient_v(184, 6, 128, 128, T(blend(RGB(255, 226, 170), RGB(80, 60, 50), flick)), T(RGB(150, 110, 90)));
        rect(214, 26, 44, 108, T(RGB(126, 88, 62)));  /* the door across the hall */
        circle_fill(250, 82, 2, T(RGB(230, 200, 120)));
        rect(174, 134, 148, 8, T(RGB(150, 116, 90)));  /* light spilling onto the floor */
        for (int i = 0; i < 3; i++)                   /* coat hooks, and a hoodie that is not yours */
            circle_fill(24 + i * 22, 30, 2, T(RGB(200, 180, 140)));
        u16 hood = T(RGB(141, 136, 166)), hood_d = T(RGB(97, 92, 124));
        circle_fill(46, 38, 7, hood);
        circle_fill(46, 39, 4, hood_d);
        rect(35, 42, 22, 30, hood);
        rect(31, 44, 5, 26, hood_d);                  /* sleeves hanging */
        rect(56, 44, 5, 26, hood_d);
        hline(39, 60, 14, hood_d);                    /* the pocket */
        vline(44, 44, 8, T(C_WHITE));
        vline(48, 44, 8, T(C_WHITE));
        rect(10, 112, 90, 18, T(RGB(120, 84, 60)));    /* the shoe shelf */
        hline(10, 112, 90, T(RGB(160, 118, 84)));
        break;
    }
    case DL_HALLWAY: {                                /* the corridor outside the flats */
        room(RGB(128, 110, 100), RGB(90, 78, 76), RGB(104, 46, 58));
        for (int d = 0; d < 3; d++) {
            int x = 14 + d * 108;
            rect(x - 3, 26, 56, 106, T(RGB(90, 66, 52)));
            rect(x, 30, 50, 102, T(d == 1 ? RGB(150, 108, 76) : RGB(136, 96, 68)));
            round_box(x + 17, 38, 16, 9, T(RGB(220, 200, 150)), T(RGB(60, 44, 36)));
            static const char *const num[3] = { "3A", "3B", "3C" };
            text(x + 19, 39, num[d], T(RGB(230, 210, 160)));
            circle_fill(x + 42, 84, 2, T(RGB(230, 200, 120)));
        }
        for (int i = 0; i < 3; i++) {                 /* ceiling lights; the middle one flickers */
            int on = i != 1 || (f / 3) % 61 > 2;
            glow(58 + i * 104, 4, 16, on ? RGB(255, 228, 170) : RGB(90, 80, 70));
        }
        hline(0, 150, SCREEN_W, T(RGB(150, 70, 80)));
        break;
    }
    case DL_KITCHEN: {
        room(RGB(150, 128, 120), RGB(112, 94, 96), RGB(150, 104, 70));
        for (int i = 0; i < 4; i++) {                 /* upper cabinets */
            rect(4 + i * 27, 8, 25, 38, T(RGB(170, 128, 92)));
            rect_line(7 + i * 27, 11, 19, 32, T(RGB(140, 100, 70)));
        }
        window(128, 12, 80, 58);
        rect(0, 92, SCREEN_W, 7, T(RGB(222, 214, 204)));   /* counter */
        rect(0, 99, SCREEN_W, 31, T(RGB(160, 120, 88)));
        for (int i = 0; i < 6; i++)
            vline(53 * i + 20, 100, 30, T(RGB(130, 94, 66)));
        rect(20, 80, 18, 12, T(RGB(200, 90, 90)));     /* the kettle */
        rect(24, 76, 10, 4, T(RGB(200, 90, 90)));
        rect(80, 84, 10, 8, T(RGB(236, 236, 240)));    /* a mug, still steaming */
        for (int k = 0; k < 3; k++) {
            int ph = (int)(f * 2 + (u32)k * 40) % 64;
            pset(84 + isin(ph * 8 + k * 60) / 90, 82 - ph / 4, T(blend(C_GREY, RGB(150, 128, 120), ph * 3)));
            pset(85 + isin(ph * 8 + k * 60 + 30) / 90, 80 - ph / 4, T(blend(C_GREY, RGB(150, 128, 120), ph * 4)));
        }
        vline(262, 0, 14, T(RGB(60, 50, 50)));         /* pendant lamp */
        tri_fill(250, 22, 274, 22, 262, 12, T(RGB(230, 190, 120)));
        glow(262, 26, 22, RGB(255, 220, 150));
        break;
    }
    case DL_COUCH: {
        room(RGB(92, 80, 122), RGB(66, 56, 90), RGB(84, 62, 58));
        window(20, 16, 92, 70);
        for (int k = 0; k < 10; k++) {                /* curtains */
            vline(14 + k / 3, 12, 84 - k, T(RGB(150, 90, 150)));
            vline(117 - k / 3, 12, 84 - k, T(RGB(150, 90, 150)));
        }
        glow(140, 52, 26, RGB(255, 214, 150));        /* floor lamp */
        tri_fill(128, 46, 152, 46, 140, 34, T(RGB(240, 210, 160)));
        vline(140, 46, 84, T(RGB(70, 60, 60)));
        rect_line(240, 26, 44, 34, T(RGB(170, 140, 100)));   /* a framed print */
        rect(242, 28, 40, 30, T(RGB(120, 150, 170)));
        tri_fill(242, 58, 262, 38, 282, 58, T(RGB(90, 120, 100)));
        rect(0, 102, SCREEN_W, 28, T(RGB(126, 92, 148)));   /* the couch back, behind her */
        for (int i = 0; i < 4; i++)
            round_box(6 + i * 80, 96, 72, 16, T(RGB(150, 116, 170)), T(RGB(112, 80, 132)));
        break;
    }
    case DL_STAIRWELL: {
        gradient_v(0, 0, SCREEN_W, SCREEN_H, T(RGB(92, 96, 112)), T(RGB(46, 48, 60)));
        for (int i = 0; i < 9; i++) {                 /* steps climbing to the right */
            rect(i * 24 - 10, 130 - i * 13, 70, 13, T(RGB(84, 88, 100)));
            hline(i * 24 - 10, 130 - i * 13, 70, T(RGB(130, 134, 148)));
        }
        line(0, 104, 230, 0, T(RGB(170, 160, 130)));  /* handrail */
        line(0, 105, 230, 1, T(RGB(110, 100, 80)));
        int on = (f / 2) % 97 > 1;                    /* a caged bulb with a loose connection */
        glow(60, 12, 18, on ? RGB(255, 236, 170) : RGB(110, 100, 80));
        rect_line(55, 8, 11, 10, T(RGB(60, 60, 60)));
        window(272, 20, 30, 56);
        break;
    }
    case DL_STORE: {                                  /* the 24-hour store: too bright, humming */
        gradient_v(0, 0, SCREEN_W, 130, T(RGB(236, 240, 244)), T(RGB(206, 212, 220)));
        rect(0, 130, SCREEN_W, 50, T(RGB(196, 200, 206)));
        for (int i = 0; i < 5; i++) {
            int on = i != 3 || (f / 2) % 83 > 2;
            rect(10 + i * 64, 2, 44, 4, on ? T(C_WHITE) : T(RGB(160, 164, 170)));
        }
        for (int s = 0; s < 3; s++) {                 /* shelves of snacks */
            rect(4, 30 + s * 30, 156, 3, T(RGB(150, 150, 160)));
            for (int i = 0; i < 17; i++) {
                static const u16 cols[5] = { C_PINK, C_SKY, C_GOLD, C_MINT, C_ORANGE };
                rect(6 + i * 9, 14 + s * 30, 7, 16, T(cols[(i * 3 + s) % 5]));
            }
        }
        rect(258, 10, 60, 120, T(RGB(60, 90, 140)));  /* the drinks fridge, glowing */
        rect(262, 14, 52, 112, T(RGB(150, 200, 240)));
        for (int s = 0; s < 4; s++)
            for (int i = 0; i < 6; i++)
                rect(265 + i * 8, 20 + s * 27, 5, 16, T(i % 2 ? C_MINT : RGB(90, 130, 255)));
        break;
    }
    default: {                                        /* her place, in her colours */
        u16 acc = cast[who].name_color;
        room(blend(RGB(84, 70, 104), acc, 30), RGB(58, 48, 76), RGB(90, 66, 58));
        window(22, 18, 76, 60);
        if (who == CH_NIA) {                          /* desk, monitor glow, string lights */
            for (int i = 0; i < 12; i++) {
                static const u16 cols[4] = { C_PINK, C_GOLD, C_MINT, C_SKY };
                int on = ((f / 20 + (u32)i) % 5) != 0;
                circle_fill(10 + i * 27, 6 + (i & 1), 1, on ? T(cols[i % 4]) : T(C_DIM));
            }
            rect(110, 96, 120, 6, T(RGB(120, 84, 60)));
            rect(120, 62, 44, 32, T(RGB(30, 34, 60)));
            rect(122, 64, 40, 28, T(blend(RGB(60, 90, 200), C_WHITE, 20 + (int)(f / 5 % 20))));
            rect(172, 86, 5, 10, T(C_MINT));
            rect(180, 86, 5, 10, T(RGB(90, 130, 255)));
        } else if (who == CH_MAKO) {                  /* posters and a shelf of trophies */
            rect(118, 14, 34, 46, T(C_MAGENTA));
            rect(122, 18, 26, 20, T(RGB(255, 200, 230)));
            rect(160, 20, 30, 40, T(C_VIOLET));
            rect(110, 76, 110, 4, T(RGB(150, 110, 80)));
            for (int i = 0; i < 4; i++) {
                rect(118 + i * 26, 66, 8, 8, T(C_GOLD));
                rect(120 + i * 26, 74, 4, 2, T(C_GOLD));
            }
        } else {                                      /* bookshelves, case files, a plant */
            for (int s = 0; s < 3; s++) {
                rect(110, 16 + s * 30, 100, 3, T(RGB(150, 120, 90)));
                for (int i = 0; i < 16; i++)
                    rect(112 + i * 6, 2 + s * 30 + (i % 3), 5, 14 - (i % 3), T(blend(RGB(70, 90, 110), acc, i * 13 % 90)));
            }
            rect(226, 100, 30, 26, T(RGB(170, 110, 80)));   /* the plant */
            ellipse_fill(241, 90, 16, 12, T(RGB(80, 150, 100)));
            ellipse_fill(234, 84, 8, 8, T(RGB(100, 170, 110)));
        }
        break;
    }
    }
    if ((weather & DW_DARK) && state == ST_SCENE)     /* the power is out: her laptop is the only light */
        glow(por.x / 256 + 64, 128, 34, RGB(120, 160, 255));
}

/* the front door, in front of her, while it is shut or still swinging open */
static void draw_door(void)
{
    if (scene->loc != DL_DOORWAY || door >= 16)
        return;
    int w = 128 * (16 - door) / 16, bump = door_bump > 0 ? (door_bump & 2 ? 1 : -1) : 0;
    u16 wood = T(blend(RGB(150, 104, 72), RGB(96, 64, 46), door * 10)), dark = T(RGB(104, 70, 50));
    rect(184 + bump, 6, w, 128, wood);
    if (w > 30) {
        rect_line(192 + bump, 16, w - 16, 48, dark);
        rect_line(192 + bump, 74, w - 16, 50, dark);
        circle_fill(184 + w / 2 + bump, 40, 2, T(RGB(30, 24, 24)));   /* the peephole */
    }
    circle_fill(184 + w - 10 + bump, 72, 3, T(RGB(230, 200, 120)));
}

/* ---------------------------------------------------------------- dialogue box and choices */

static void draw_box(const char *name, u16 name_col, const char *s, int nshow, u16 col)
{
    round_box(4, 130, 312, 48, C_WHITE, RGB(36, 30, 58));
    rect_blend(6, 132, 308, 3, C_WHITE, 20);
    if (name) {
        round_box(10, 123, text_w(name) + 10, 12, C_WHITE, name_col);
        text(15, 125, name, C_INK);
    }
    int starts[4], lens[4];
    int n = wrap_text(s, 296, starts, lens, 4);
    for (int i = 0; i < n && i < 3; i++) {
        int take = nshow - starts[i];
        if (take <= 0)
            break;
        text_n(12, 139 + i * LINE_H, s + starts[i], imin(take, lens[i]), col);
    }
}

static void draw_choices(void)
{
    const char *prompt = date_steps[ip].text;
    int starts[2], lens[2];
    int prows = prompt[0] ? wrap_text(prompt, 296, starts, lens, 2) : 0;
    int h = 8 + prows * LINE_H + 2;
    for (int k = 0; k < nopt; k++)
        h += opt_rows[k] * LINE_H + 4;
    int y0 = SCREEN_H - 2 - h;
    round_box(4, y0, 312, h, C_PINK, RGB(36, 30, 58));
    if (prows)
        text_wrapped(12, y0 + 5, 296, prompt, C_GOLD, 2);
    int y = y0 + 6 + prows * LINE_H + 2, ys[4];
    for (int k = 0; k < nopt; k++) {
        ys[k] = y;
        y += opt_rows[k] * LINE_H + 4;
    }
    int target = ys[sel] << 4;                        /* the highlight slides to the chosen row */
    bar_y = bar_y ? bar_y + (target - bar_y) / 3 : target;
    rect_blend(8, (bar_y >> 4) - 2, 304, opt_rows[sel] * LINE_H + 3, C_PINK, 70);
    for (int k = 0; k < nopt; k++) {
        if (k == sel)
            spr(SPR_PAW, 10 + (int)(frame_count / 8 % 2), ys[k] + 1, 0);
        text_wrapped(24, ys[k], 288, date_steps[opt_idx[k]].text, k == sel ? C_WHITE : C_GREY, 2);
    }
}

/* ---------------------------------------------------------------- the three of them, head to toe (title and pick) */

static const int title_x[3] = { 128, 190, 252 }, pick_x[3] = { 36, 132, 228 };
#define FIG_Y 46
static int picked_now = -1, hop[3], dimmed[3];        /* who stands in the light; her little hop; the others' shadow */

static void trio_place(int picked)
{
    if (picked >= 0 && picked != picked_now)
        hop[picked] = 14;
    if (picked < 0 && picked_now >= 0)                /* back to standing together */
        for (int w = 0; w < 3; w++)
            portrait_cue(&trio[w], EX_NEUTRAL, EM_UP, TM_SWAY, -1, GE_NONE, -1, 0);
    picked_now = picked;
}

static void trio_update(void)
{
    for (int w = 0; w < 3; w++) {
        portrait_update(&trio[w]);
        if (rnd() % 420 == 0)
            portrait_flick_tail(&trio[w]);
        if (hop[w] > 0)
            hop[w]--;
        int target = picked_now >= 0 && w != picked_now ? 130 : 0, d = target - dimmed[w];
        dimmed[w] += d / 4 + (d > 0) - (d < 0);       /* ease into and out of the shadow */
    }
}

static void floor_blend(int cx, int cy, int rx, int ry, u16 c, int alpha)   /* a flat ellipse of light or shadow */
{
    for (int y = -ry; y <= ry; y++) {
        int yy = cy + y;
        if (yy < 0 || yy >= SCREEN_H)
            continue;
        int w = rx * (int)isqrt((u32)(ry * ry - y * y) << 8) / (ry << 4);
        u16 *px = fb + yy * SCREEN_W;
        for (int x = imax(0, cx - w); x <= imin(SCREEN_W - 1, cx + w); x++)
            px[x] = blend(px[x], c, alpha);
    }
}

static void trio_draw(const int *xs)                  /* left to right, so each tail tucks behind the next of them */
{
    for (int w = 0; w < 3; w++) {
        int x = xs[w];
        if (picked_now == w)                          /* a warm pool of light at her feet */
            floor_blend(x + 28, FIG_Y + 121, 27, 6, RGB(255, 214, 150), 60 + isin((int)frame_count * 2) / 16);
        floor_blend(x + 28, FIG_Y + 122, 15, 3, RGB(12, 8, 20), 120);
        int h = hop[w] ? -(hop[w] * (14 - hop[w])) / 12 : 0;
        figure_draw(&trio[w], x, FIG_Y + h, 256, dimmed[w]);
    }
}

static void trio_pick(int w)
{
    for (int k = 0; k < 3; k++)
        portrait_cue(&trio[k], k == w ? (milestones_done(k) >= 3 ? EX_HAPPY : EX_NEUTRAL) : EX_NEUTRAL,
                     k == w ? EM_PERK : EM_UP, k == w ? TM_CURL : TM_SWAY, -1, GE_NONE, -1, 0);
    trio_place(w);
}

/* ---------------------------------------------------------------- the scene */

static void date_enter(void)
{
    music(MUS_DATE);
    if (!black)
        music_vol = audio_music_volume;
    state = ST_TITLE;
    sel = save.date.active ? 0 : 1;
    t = 0;
    for (int w = 0; w < 3; w++) {
        save.date.outfit_unlocked[w] |= 1;
        if (!(save.date.outfit_unlocked[w] & (1 << save.date.outfit_worn[w])))
            save.date.outfit_worn[w] = 0;
        portrait_init(&trio[w], w, save.date.outfit_worn[w]);
    }
    trio_place(-1);
    for (int w = 0; w < 3; w++)
        portrait_snap(&trio[w]);
    platform_log("date: title");
}

static void update_title(void)
{
    trio_update();
    if (any_repeat(BTN_DOWN)) { sel = (sel + 1) % 5; sfx(SFX_MOVE); }
    if (any_repeat(BTN_UP)) { sel = (sel + 4) % 5; sfx(SFX_MOVE); }
    if (any_pressed(BTN_B | BTN_START)) { sfx(SFX_BACK); scene_fade_to(&scene_menu); return; }
    if (!any_pressed(BTN_A))
        return;
    if (sel == 0 && !save.date.active) { sfx(SFX_BUZZ); return; }
    sfx(SFX_OK);
    switch (sel) {
    case 0: state = ST_EVENING; t = 0; break;
    case 1: new_story(); state = ST_EVENING; t = 0; break;
    case 2: state = ST_WARDROBE; ward_who = 0; portrait_init(&mini, 0, save.date.outfit_worn[0]); break;
    case 3: state = ST_ALBUM; album_sel = 0; break;
    default: scene_fade_to(&scene_menu); break;
    }
}

static void update_pick(void)
{
    trio_update();
    if (t == 1)
        trio_pick(sel);
    int moved = 0;
    if (any_repeat(BTN_RIGHT)) { sel = (sel + 1) % 3; moved = 1; }
    if (any_repeat(BTN_LEFT)) { sel = (sel + 2) % 3; moved = 1; }
    if (moved) { sfx(SFX_MOVE); trio_pick(sel); }
    if (any_pressed(BTN_B)) { sfx(SFX_BACK); state = ST_TITLE; sel = 0; trio_place(-1); return; }
    if (any_pressed(BTN_A)) {
        const DScene *s = next_scene(sel);
        if (s) { sfx(SFX_OK); start_scene(s); }
    }
}

static void black_step(void)                          /* the fade to black and back, with the music dipping under it */
{
    if (black_dir > 0 && black < 32)
        black++;
    else if (black_dir < 0 && --black <= 0)
        black = black_dir = 0;
    audio_music_volume = music_vol - music_vol * black / 48;
}

static void update_scene(void)
{
    portrait_update(&por);
    black_step();
    if (heart_t > 0) heart_t--;
    if (flash_t > 0) flash_t--;
    if (fade_t > 0) fade_t--;
    if (door_bump > 0) door_bump--;
    if (thunder_t > 0 && --thunder_t == 0) sfx(SFX_THUNDER);
    if (cue_step >= 0 && --cue_t <= 0) { apply_cues(&date_steps[cue_step]); cue_step = -1; }
    if (knock_n + (knock_soft ? 1 : 0) > 0) {        /* knuckles on the door, then the door */
        if (knock_t++ % 11 == 0) {
            if (knock_n > 0) { knock_n--; sfx(SFX_KNOCK); door_bump = 6; }
            else { knock_soft = 0; sfx(SFX_KNOCK_SOFT); door_bump = 3; }
            if (knock_n == 0 && !knock_soft) door_t = 1;
        }
    } else if (door < 16 && (door_t ? ++door_t > 26 : shown_x2 > 0 && date_steps[ip].kind == DS_LINE &&
                             date_steps[ip].who != 2)) {
        door++;
        if (door == 1) sfx(SFX_WHOOSH);
    }
    if (any_pressed(BTN_START)) {                     /* leave: this scene does not count, nothing is kept */
        sfx(SFX_BACK);
        platform_log("date: left a scene");
        state = ST_TITLE;
        sel = 0;
        black = black_dir = 0;
        audio_music_volume = music_vol;
        trio_place(-1);
        return;
    }
    if (choosing) {
        if (any_repeat(BTN_DOWN)) { sel = (sel + 1) % nopt; sfx(SFX_MOVE); }
        if (any_repeat(BTN_UP)) { sel = (sel + nopt - 1) % nopt; sfx(SFX_MOVE); }
        if (any_pressed(BTN_A)) pick_option(sel);
        return;
    }
    const DStep *st = &date_steps[ip];
    int who = you_line ? 1 : st->who, len = str_len(st->text);
    if (who == 3 && black < 32)                       /* the line waits until the screen is dark */
        return;
    if (shown_x2 < len * 2) {
        int before = shown_x2 / 2;
        shown_x2 += who == 2 ? 4 : 3;
        int now = imin(shown_x2 / 2, len - 1);
        if (who == 0 && now / 2 != before / 2 && st->text[now] != ' ')
            sfx_voice(cast[scene->who].voice + (int)(rnd() % 3));
        if (any_pressed(BTN_A | BTN_B))
            shown_x2 = len * 2;
    } else if (any_pressed(BTN_A | BTN_B)) {
        sfx(SFX_BLIP);
        if (you_line) {
            you_line = 0;
            ip++;
            step_begin();
        } else {
            advance();
        }
    }
    por.talking = who == 0 && shown_x2 < len * 2 && state == ST_SCENE;
}

static void update_wardrobe(void)
{
    portrait_update(&mini);
    if (rnd() % 160 == 0) portrait_flick_tail(&mini);
    int who = ward_who, o = save.date.outfit_worn[who];
    if (any_repeat(BTN_RIGHT) || any_repeat(BTN_LEFT)) {
        ward_who = (ward_who + (any_repeat(BTN_RIGHT) ? 1 : 2)) % 3;
        portrait_init(&mini, ward_who, save.date.outfit_worn[ward_who]);
        sfx(SFX_MOVE);
        return;
    }
    if (any_repeat(BTN_UP) || any_repeat(BTN_DOWN)) {
        int dir = any_repeat(BTN_UP) ? OUTFITS - 1 : 1;
        for (int k = 0; k < OUTFITS; k++) {           /* the next outfit she has */
            o = (o + dir) % OUTFITS;
            if (save.date.outfit_unlocked[who] & (1 << o)) break;
        }
        if (o != save.date.outfit_worn[who]) {
            save.date.outfit_worn[who] = (u8)o;
            portrait_init(&mini, who, o);
            portrait_cue(&mini, EX_HAPPY, EM_PERK, TM_CURL, -1, GE_NONE, 1, 0);
            portrait_flick_tail(&mini);
            sfx(SFX_POP);
        } else {
            sfx(SFX_BUZZ);
        }
    }
    if (any_pressed(BTN_B | BTN_A)) {
        sfx(SFX_BACK);
        state = ST_TITLE;
        sel = 2;
        portrait_init(&trio[who], who, save.date.outfit_worn[who]);
        trio_place(-1);
        portrait_snap(&trio[who]);
    }
}

static void update_album(void)
{
    if (any_repeat(BTN_RIGHT)) { album_sel = (album_sel + 1) % 30; sfx(SFX_MOVE); }
    if (any_repeat(BTN_LEFT)) { album_sel = (album_sel + 29) % 30; sfx(SFX_MOVE); }
    if (any_repeat(BTN_DOWN)) { album_sel = (album_sel + 5) % 30; sfx(SFX_MOVE); }
    if (any_repeat(BTN_UP)) { album_sel = (album_sel + 25) % 30; sfx(SFX_MOVE); }
    if (any_pressed(BTN_B | BTN_A)) { sfx(SFX_BACK); state = ST_TITLE; sel = 3; }
}

static void date_update(void)
{
    t++;
    switch (state) {
    case ST_TITLE: update_title(); break;
    case ST_EVENING:
        if (t > 110 || (t > 15 && any_pressed(BTN_A))) {
            state = ST_PICK;
            sel = save.date.last_pick % 3;
            t = 0;
        }
        break;
    case ST_PICK: update_pick(); break;
    case ST_SCENE: update_scene(); break;
    case ST_AFTER:
        portrait_update(&por);
        black_step();
        if (new_outfit >= 0) portrait_update(&mini);
        if (t > 20 && any_pressed(BTN_A | BTN_B)) {
            sfx(SFX_OK);
            black = black_dir = 0;
            audio_music_volume = music_vol;
            state = save.date.active ? ST_EVENING : ST_TITLE;
            sel = 0;
            t = 0;
            for (int w = 0; w < 3; w++)
                portrait_init(&trio[w], w, save.date.outfit_worn[w]);
            trio_place(-1);
            for (int w = 0; w < 3; w++)
                portrait_snap(&trio[w]);
        }
        break;
    case ST_WARDROBE: update_wardrobe(); break;
    case ST_ALBUM: update_album(); break;
    }
}

static void draw_hearts(int x, int y, int who)
{
    for (int k = 1; k <= 6; k++)
        spr((save.date.milestones[who] >> k) & 1 ? SPR_HEART : SPR_HEART_EMPTY, x + (k - 1) * 9, y, 0);
}

static void date_draw(void)
{
    char b[64];
    switch (state) {
    case ST_TITLE: {                                 /* the three of them in the hallway; the menu to one side */
        weather = 0; dim = 60;
        draw_bg(DL_HALLWAY, 0);
        dim = 0;
        trio_draw(title_x);
        rect_blend(4, 4, 126, 46, RGB(20, 16, 36), 150);
        text_big(10, 8, "Thin Walls", 2, C_PINK, C_INK);
        text_sh(10, 28, "Nia, Mako and Shio.", C_GREY, C_INK);
        text_sh(10, 38, "Fourteen evenings.", C_GREY, C_INK);
        static const char *const items[5] = { "Continue", "New story", "Wardrobe", "Album", "Back to Nia's desk" };
        round_box(6, 104, 116, 56, C_PINK, RGB(36, 30, 58));
        for (int i = 0; i < 5; i++) {
            u16 c = (i == 0 && !save.date.active) ? C_DIM : i == sel ? C_WHITE : C_GREY;
            if (i == sel) spr(SPR_PAW, 10, 109 + i * 10, 0);
            text(22, 108 + i * 10, items[i], c);
        }
        if (save.date.active) {
            str_cpy(str_int(str_cpy(b, "Evening "), save.date.evening), " of 14");
            text(10, 166, b, C_DIM);
        }
        break;
    }
    case ST_EVENING: {
        gradient_v(0, 0, SCREEN_W, SCREEN_H, RGB(20, 16, 40), RGB(44, 30, 64));
        int a = imin(256, t * 8);
        str_int(str_cpy(b, "Evening "), save.date.evening);
        text_big(160 - text_big_w(b, 3) / 2, 62, b, 3, blend(RGB(20, 16, 40), C_GOLD, a), C_INK);
        const char *sub = save.date.evening >= date_evenings ? "The last evening. Whose door?"
                        : save.date.evening == 1 ? "The first evening. The walls are thin."
                        : "The building settles. Someone is still awake.";
        text_center(160, 96, sub, blend(RGB(20, 16, 40), C_GREY, imin(256, imax(0, t - 20) * 8)), C_INK);
        for (int i = 0; i < 14; i++)                  /* the fortnight so far */
            circle_fill(160 - 13 * 6 + i * 12, 118, 3, i < save.date.evening ? C_PINK : RGB(60, 50, 84));
        break;
    }
    case ST_PICK: {                                  /* whose door tonight: head to toe, the words above them */
        weather = 0; dim = 60;
        draw_bg(DL_HALLWAY, 0);
        dim = 0;
        trio_draw(pick_x);
        static const char *const hooks[3][2] = {
            { "The kitchen light is on. So is her laptop.", "Nia's door is cracked open. Laptop glow." },
            { "Three sharp knocks. One softer.", "Three sharp knocks. One softer. You know the rhythm." },
            { "One knock. Then nothing.", "One knock. Exactly one." },
        };
        round_box(4, 4, 312, 40, cast[sel].name_color, RGB(36, 30, 58));
        text_sh(12, 9, names[sel], cast[sel].name_color, C_INK);
        draw_hearts(12 + text_w(names[sel]) + 8, 9, sel);
        str_cpy(str_int(str_cpy(b, "Evening "), save.date.evening), save.date.evening >= date_evenings ?
                " - the last one" : " of 14");
        text(308 - text_w(b), 9, b, C_GOLD);
        text(12, 21, hooks[sel][save.date.milestones[sel] & 1], C_WHITE);
        text(12, 32, "\x05 left/right to choose   A: answer   B: back", C_DIM);
        break;
    }
    case ST_SCENE:
    case ST_AFTER: {
        weather = scene->weather;
        draw_bg(scene->loc, scene->who);
        if (door > 0 || scene->loc != DL_DOORWAY)
            portrait_draw(&por);
        draw_door();
        if (flash_t > 0)
            rect_blend(0, 0, SCREEN_W, SCREEN_H, C_WHITE, flash_t * 14);
        str_int(str_cpy(b, "Evening "), save.date.evening);
        text_sh(6, 4, b, C_GOLD, C_INK);
        text_sh(6, 14, loc_names[scene->loc], C_GREY, C_INK);
        if (heart_t > 0) {                            /* a small heart for something she will remember */
            int rise = (80 - heart_t) / 2;
            spr(SPR_HEART, heart_x + isin(heart_t * 6) / 64, 60 - rise, 0);
        }
        if (fade_t > 0)
            rect_blend(0, 0, SCREEN_W, SCREEN_H, RGB(10, 8, 20), fade_t * 16);
        if (black > 0)                                /* fade to black */
            rect_blend(0, 0, SCREEN_W, SCREEN_H, RGB(4, 3, 10), imin(256, black * 8));
        if (state == ST_SCENE && !choosing && !you_line && date_steps[ip].kind == DS_LINE && date_steps[ip].who == 3) {
            const char *s = date_steps[ip].text;      /* the words over black */
            if (black >= 32) {                        /* large when it fits, typed like any line */
                char buf[48];
                int n = imin(imin(shown_x2 / 2, str_len(s)), 47), scale = text_big_w(s, 2) <= 300 ? 2 : 1;
                mem_copy(buf, s, (u32)n);
                buf[n] = 0;
                text_big(160 - text_big_w(s, scale) / 2, 88 - 4 * scale, buf, scale, RGB(255, 226, 176), RGB(4, 3, 10));
                if (shown_x2 / 2 >= str_len(s) && (frame_count / 16) % 2 == 0)
                    circle_fill(160, 100, 1, C_DIM);
            }
            break;
        }
        if (state == ST_AFTER) {                      /* the evening's card, where the dialogue was: she stays in view */
            round_box(4, 130, 312, 48, C_GOLD, RGB(36, 30, 58));
            text_sh(12, 135, save.date.active ? "The evening ends." : "The story ends.", C_GOLD, C_INK);
            for (int i = 0; i < nnotes; i++)
                text(12, 146 + i * 10, notes[i], i ? C_WHITE : C_GREY);
            if (new_outfit >= 0) {                    /* the new outfit, on her */
                mini.tx = mini.x = 244 << 8; mini.ty = mini.y = 128 << 8; mini.ts = mini.s = 192;
                portrait_draw(&mini);
                for (int k = 0; k < 3; k++)
                    text(244 + (int)((frame_count / 4 + (u32)k * 17) % 48), 130 + (int)((frame_count / 3 + (u32)k * 11) % 40),
                         "\x02", C_GOLD);
            }
            if ((frame_count / 16) % 2 == 0)
                text(304, 167, "\x06", C_PINK);
            break;
        }
        if (choosing) {
            draw_choices();
        } else {
            const DStep *st = &date_steps[ip];
            int who = you_line ? 1 : st->who;
            const char *nm = who == 0 ? names[scene->who] : who == 1 ? "You" : 0;
            u16 col = who == 2 ? RGB(255, 226, 176) : C_WHITE;
            draw_box(nm, who == 0 ? cast[scene->who].name_color : C_SKY, st->text, shown_x2 / 2, col);
            if (shown_x2 / 2 >= str_len(st->text) && (frame_count / 16) % 2 == 0)
                text(304, 167, "\x06", C_PINK);
        }
        break;
    }
    case ST_WARDROBE: {
        weather = 0; dim = 0;
        draw_bg(DL_HER_ROOM, ward_who);
        mini.tx = 150 << 8; mini.ty = 14 << 8; mini.ts = 512;
        portrait_draw(&mini);
        text_big(10, 6, "Wardrobe", 2, C_PINK, C_INK);
        round_box(6, 30, 132, 20 + OUTFITS * 12, cast[ward_who].name_color, RGB(36, 30, 58));
        text_sh(14, 35, names[ward_who], cast[ward_who].name_color, C_INK);
        for (int o = 0; o < OUTFITS; o++) {
            int got = save.date.outfit_unlocked[ward_who] & (1 << o), on = o == save.date.outfit_worn[ward_who];
            if (on) spr(SPR_PAW, 12, 48 + o * 12, 0);
            text(26, 47 + o * 12, got ? portrait_outfit_name(ward_who, o) : "? a milestone away", on ? C_WHITE : got ?
                 C_GREY : C_DIM);
        }
        round_box(4, 150, 312, 28, C_WHITE, RGB(36, 30, 58));
        text(12, 155, "She wears it in your evenings together.", C_GREY);
        text(12, 166, "\x05 left/right: who   up/down: outfit   B: back", C_DIM);
        break;
    }
    case ST_ALBUM: {
        gradient_v(0, 0, SCREEN_W, SCREEN_H, RGB(46, 34, 72), RGB(22, 18, 38));
        text_big(10, 6, "Album", 2, C_PINK, C_INK);
        int who = album_sel / 10, k = album_sel % 10;
        for (int w = 0; w < 3; w++) {
            text_sh(12 + w * 104, 30, names[w], cast[w].name_color, C_INK);
            for (int i = 0; i < 10; i++) {
                int got = i < 7 ? (save.date.album[w] >> i) & 1 : (save.date.endings[w] >> (i - 7)) & 1;
                int x = 12 + w * 104 + (i % 5) * 18, y = 42 + (i / 5) * 18;
                round_box(x, y, 16, 16, w * 10 + i == album_sel ? C_WHITE : C_DIM,
                          got ? blend(cast[w].name_color, RGB(36, 30, 58), 90) : RGB(40, 34, 60));
                if (got) spr(i >= 7 ? SPR_HEART : SPR_STAR, x + 4, y + 4, 0);
            }
        }
        int got = k < 7 ? (save.date.album[who] >> k) & 1 : (save.date.endings[who] >> (k - 7)) & 1;
        const DScene *s = k == 0 ? find_scene(who, DK_INTRO, 0) : k < 7 ? find_scene(who, DK_MILESTONE, k)
                        : find_scene(who, DK_ENDING, k - 7);
        static const char *const slot[10] = { "The first evening", "Milestone 1", "Milestone 2", "Milestone 3",
                                              "Milestone 4", "Milestone 5", "Milestone 6", "Ending: hoped for",
                                              "Ending: friends", "Ending: goodbye" };
        round_box(6, 84, 308, 76, cast[who].name_color, RGB(36, 30, 58));
        text_sh(14, 89, slot[k], cast[who].name_color, C_INK);
        text_wrapped(14, 101, 292, got && s ? s->desc : "Not reached yet.", got ? C_WHITE : C_DIM, 5);
        text(10, 166, "\x05 arrows to browse   B: back", C_DIM);
        break;
    }
    }
}

const Scene scene_date = { date_enter, date_update, date_draw, "date" };
