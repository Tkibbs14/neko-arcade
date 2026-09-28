/* Neko Arcade: scene manager, fades, the per-frame entry point used by every platform, and the playtest
 * diagnostics (frame timing, the model's time budget, the performance overlay and the periodic log line). */
#include "engine.h"
#include "save.h"
#include "nekolm.h"
#include "live.h"

static const Scene *cur_scene, *next_scene;
static int fade_t;            /* 0 = none; 1..FADE = fading out; FADE+1..2*FADE = fading in */
static int started;
#define FADE 12

/* The core's own work per frame should stay under WORK_TARGET_US so the frontend (video, audio) still fits in
 * a 16.7 ms frame; the model gets what the game leaves of it. A frame whose work passes LATE_US is counted. */
#define WORK_TARGET_US 10000
#define LATE_US 14000

int perf_overlay;
static u32 perf_frames, perf_late, perf_work_sum, perf_work_max, perf_ai_sum, perf_ai_max;
static u16 hist[64];          /* recent frames' work in 1/10 ms, for the overlay graph */
static char logline[256];

char *log_buf(void)
{
    logline[0] = 0;
    return logline;
}

const Scene *current_scene(void) { return cur_scene; }

void scene_set(const Scene *s)
{
    cur_scene = s;
    char *b = log_buf();
    str_cpy(str_cpy(b, "scene "), s->name ? s->name : "?");
    platform_log(logline);
    if (s->enter)
        s->enter();
}

void scene_fade_to(const Scene *s)
{
    if (fade_t)
        return;
    next_scene = s;
    fade_t = 1;
}

void game_init(void)
{
    save_check();
    started = 0;
    fade_t = 0;
    frame_count = 0;
    /* One throwaway model step pages the weights in now, at the loading screen, instead of the first time a
     * character speaks mid-game (on the stick that first touch reads them from the SD card). */
    nlm_claim();
    nlm_forward(10);
    nlm_claim();
    char *b = log_buf();
    nlm_pick_kernel(b);                            /* the fastest exact dot product on this CPU */
    platform_log(b);
    scene_set(&scene_menu);
}

static char *put(char *p, const char *label, u32 v)
{
    return str_int(str_cpy(p, label), (int)v);
}

/* Every 10 seconds of play: how the frames and the model did. */
static void log_perf(void)
{
    char *p = log_buf();
    p = str_cpy(p, "perf ");
    p = str_cpy(p, cur_scene && cur_scene->name ? cur_scene->name : "?");
    p = put(p, " work_avg_us=", perf_work_sum / perf_frames);
    p = put(p, " work_max_us=", perf_work_max);
    p = put(p, " late=", perf_late);
    p = put(p, " ai_avg_us=", perf_ai_sum / perf_frames);
    p = put(p, " ai_max_us=", perf_ai_max);
    p = put(p, " slices=", nlm_stat_slices);
    p = put(p, " slice_avg_us=", nlm_stat_slices ? nlm_stat_us / nlm_stat_slices : 0);
    p = put(p, " slice_max_us=", nlm_stat_max_us);
    p = put(p, " lines_composed=", live_stat_composed);
    p = put(p, " lines_whole=", live_stat_whole);
    put(p, " budget_us=", (u32)nlm_frame_budget_us);
    platform_log(logline);
    perf_frames = perf_late = perf_work_sum = perf_work_max = perf_ai_sum = perf_ai_max = 0;
    nlm_stat_slices = nlm_stat_us = nlm_stat_max_us = 0;
    live_stat_composed = live_stat_whole = 0;
}

/* SELECT: frame time and the model's share, and a graph of the last 64 frames (red above the late line). */
static void draw_perf(void)
{
    int x0 = SCREEN_W - 104, y0 = 2;
    rect_blend(x0, y0, 102, 40, C_INK, 200);
    u32 n = perf_frames ? perf_frames : 1;
    char b[48], *p = b;
    p = str_cpy(p, "frame ");
    p = str_int(p, (int)(perf_work_sum / n / 1000));
    p = str_cpy(p, ".");
    p = str_int(p, (int)(perf_work_sum / n / 100 % 10));
    p = str_cpy(p, "ms ai ");
    p = str_int(p, (int)(perf_ai_sum / n / 1000));
    p = str_cpy(p, ".");
    str_int(p, (int)(perf_ai_sum / n / 100 % 10));
    text(x0 + 3, y0 + 2, b, C_WHITE);
    p = str_cpy(b, "late ");
    p = str_int(p, (int)perf_late);
    p = str_cpy(p, " slice ");
    p = str_int(p, (int)(nlm_stat_slices ? nlm_stat_us / nlm_stat_slices : 0));
    str_cpy(p, "us");
    text(x0 + 3, y0 + 11, b, C_GREY);
    for (int i = 0; i < 64; i++) {
        int v = hist[(frame_count + 1 + (u32)i) & 63];            /* oldest on the left */
        int h = imin(18, v / 10);
        vline(x0 + 3 + i, y0 + 38 - h, h, v * 100 > LATE_US ? C_RED : v * 100 > WORK_TARGET_US ? C_GOLD : C_MINT);
    }
    hline(x0 + 3, y0 + 38 - LATE_US / 1000, 64, C_RED);
}

void game_frame(u16 p1, u16 p2)
{
    u32 t0 = time_us();
    nlm_frame_us = 0;
    if (!started) {
        /* The frontend has filled the save by now (libretro loads SRAM after the game starts). */
        save_check();
        save.boots++;
        started = 1;
        char *b = log_buf();
        str_cpy(str_int(str_cpy(b, "session start, build " __DATE__ " " __TIME__ ", boot "), save.boots), "");
        platform_log(logline);
    }
    input_update(p1, p2);
    if (any_pressed(BTN_SELECT))
        perf_overlay ^= 1;
    if (!fade_t || fade_t > FADE)
        cur_scene->update();
    cur_scene->draw();
    if (fade_t) {
        int a = fade_t <= FADE ? fade_t * 256 / FADE : (2 * FADE - fade_t) * 256 / FADE;
        rect_blend(0, 0, SCREEN_W, SCREEN_H, C_NIGHT, a);
        if (fade_t == FADE)
            scene_set(next_scene);
        if (++fade_t > 2 * FADE)
            fade_t = 0;
    }
    if (perf_overlay)
        draw_perf();
    if (t0) {                                          /* the next frame's model budget, and the stats */
        u32 work = time_us() - t0, own = work - nlm_frame_us;
        nlm_frame_budget_us = iclamp(WORK_TARGET_US - (int)own, 1500, 6000);
        perf_frames++;
        perf_work_sum += work;
        perf_ai_sum += nlm_frame_us;
        if (work > perf_work_max) perf_work_max = work;
        if (nlm_frame_us > perf_ai_max) perf_ai_max = nlm_frame_us;
        if (work > LATE_US) perf_late++;
        hist[frame_count & 63] = (u16)imin(9999, (int)(work / 100));
        if (perf_frames >= 600)
            log_perf();
    }
    frame_count++;
    /* Play time reaches the save every five minutes, not every frame: the frontend writes the save to the SD card
     * whenever it changes, and a change every frame meant a card write every 10 s (a stall on the stick). */
    static u32 unsaved_frames;
    if (++unsaved_frames >= 60 * 60 * 5) {
        save.play_frames += unsaved_frames;
        unsaved_frames = 0;
    }
}
