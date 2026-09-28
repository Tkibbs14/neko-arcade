/* Contact sheet of Thin Walls (build 06) screens, rendered by the game's own code: every location, the door
 * opening on a knock, lightning, the power cut, a choice, the reward screens. It includes src/date.c to reach its
 * state. Built and converted by tools/datelab.sh -> build/date_lab.png. */
#include <stdio.h>
#include "../src/date.c"

u32 time_us(void) { return 0; }
void platform_log(const char *line) { printf("  %s\n", line); }
char *log_buf(void) { static char b[256]; b[0] = 0; return b; }
void scene_fade_to(const Scene *s) { (void)s; }
const Scene scene_menu = { 0, 0, 0, "hub" };
void sfx(int id) { (void)id; }
void sfx_voice(int pitch) { (void)pitch; }
void music(int track) { (void)track; }
int audio_music_volume = 150;

enum { COLS = 3, CW = SCREEN_W, CH = SCREEN_H, ROWS = 7 };
static u8 sheet[ROWS * (CH + 4)][COLS * (CW + 4)][3];
static int nshots;

static void frame(u16 buttons)
{
    frame_count++;
    input_update(buttons, 0);
    date_update();
}

static void run(int n) { for (int i = 0; i < n; i++) frame(0); }
static void press(u16 b) { frame(b); frame(0); }

static void shot(const char *label)
{
    if (nshots >= ROWS * COLS)
        return;
    date_draw();
    text(SCREEN_W - text_w(label) - 4, 26, label, C_GOLD);
    int ox = (nshots % COLS) * (CW + 4), oy = (nshots / COLS) * (CH + 4);
    for (int y = 0; y < CH; y++)
        for (int x = 0; x < CW; x++) {
            u16 c = fb[y * SCREEN_W + x];
            u8 *o = sheet[oy + y][ox + x];
            o[0] = (u8)(((c >> 11) & 31) * 255 / 31);
            o[1] = (u8)(((c >> 5) & 63) * 255 / 63);
            o[2] = (u8)((c & 31) * 255 / 31);
        }
    nshots++;
    printf("shot %d: %s\n", nshots, label);
}

/* advance the scene until the step kind/speaker matches, or n lines */
static void lines(int n)
{
    for (int i = 0; i < n && state == ST_SCENE; i++) {
        if (choosing) { press(BTN_A); continue; }
        run(90);
        press(BTN_A);
    }
    run(20);
}

static void until_choice(void)
{
    for (int i = 0; i < 60 && state == ST_SCENE && !choosing; i++) {
        run(90);
        press(BTN_A);
    }
    run(10);
}

static void play_out(void)
{
    for (int i = 0; i < 200 && state == ST_SCENE; i++) {
        run(90);
        press(BTN_A);
    }
    run(30);
}

static void begin(int who, int kind, int beat)
{
    save_check();
    new_story();
    save.date.evening = 5;
    start_scene(find_scene(who, kind, beat));
}

int main(void)
{
    mem_set(&save, 0, sizeof save);
    date_enter();
    run(30);
    shot("title");
    /* a knock at the door: shut, knocking, swinging open, her */
    begin(CH_MAKO, DK_INTRO, 0);
    run(12);
    shot("knock, door shut");
    run(40);
    shot("door opening");
    lines(1);
    shot("Mako at the door");
    /* thunderstorm at the door, then lightning */
    begin(CH_SHIO, DK_HANGOUT, 6);
    run(100);
    shot("storm, Shio knocks");
    for (int i = 0; i < 12 && state == ST_SCENE && !flash_t; i++) { run(90); press(BTN_A); frame(0); }
    run(2);
    shot("lightning");
    /* power cut on the couch */
    begin(CH_NIA, DK_HANGOUT, 6);
    lines(3);
    shot("power cut");
    begin(CH_SHIO, DK_HANGOUT, 4);
    lines(2);
    shot("stairwell, late");
    begin(CH_NIA, DK_HANGOUT, 3);
    lines(2);
    shot("store, late");
    begin(CH_MAKO, DK_HANGOUT, 6);
    lines(2);
    shot("her place, rain");
    begin(CH_SHIO, DK_ENDING, 1);
    lines(2);
    shot("hallway");
    begin(CH_MAKO, DK_MILESTONE, 2);
    until_choice();
    shot("a choice");
    /* the first fade to black in the scenes: over black, then coming back */
    for (int i = 0, found = 0; i < date_scene_count && !found; i++)
        for (int k = date_scenes[i].step0; date_steps[k].kind != DS_END && !found; k++)
            if (date_steps[k].kind == DS_LINE && date_steps[k].who == 3) {
                save_check();
                new_story();
                start_scene(&date_scenes[i]);
                for (int g = 0; g < 400 && state == ST_SCENE && ip != k; g++) {   /* the real path, choosing into it */
                    if (choosing) {
                        sel = 0;
                        for (int o = 0; o < nopt; o++)
                            for (int j = opt_idx[o]; date_steps[j].kind != DS_OPT_END; j++)
                                if (j == k) sel = o;
                        press(BTN_A);
                        continue;
                    }
                    run(90);
                    press(BTN_A);
                }
                run(80);
                shot("fade to black");
                press(BTN_A);
                run(12);
                shot("back from black");
                found = 1;
            }
    /* a milestone that unlocks an outfit, then the reward screens */
    begin(CH_NIA, DK_MILESTONE, 3);
    play_out();
    run(20);
    shot("outfit unlocked");
    save.date.outfit_unlocked[CH_MAKO] = 7;
    save.date.outfit_worn[CH_MAKO] = 2;
    save.date.milestones[CH_MAKO] = 0x1F;
    state = ST_PICK; t = 0; sel = CH_MAKO;
    for (int w = 0; w < 3; w++) portrait_init(&trio[w], w, save.date.outfit_worn[w]);
    run(40);
    shot("pick: Mako");
    state = ST_WARDROBE; ward_who = CH_SHIO;
    save.date.outfit_unlocked[CH_SHIO] = 7;
    portrait_init(&mini, CH_SHIO, 0);
    press(BTN_DOWN);
    press(BTN_DOWN);
    run(40);
    shot("wardrobe");
    save.date.album[CH_NIA] = 0x0F; save.date.album[CH_SHIO] = 0x7F; save.date.endings[CH_SHIO] = 1;
    state = ST_ALBUM; album_sel = 27;
    run(5);
    shot("album");
    state = ST_EVENING; t = 0; save.date.evening = 14;
    run(60);
    shot("the last evening");

    FILE *f = fopen("build/date_lab.ppm", "wb");
    int rows = (nshots + COLS - 1) / COLS;
    fprintf(f, "P6 %d %d 255", COLS * (CW + 4), rows * (CH + 4));
    fputc(10, f);
    for (int y = 0; y < rows * (CH + 4); y++)
        fwrite(sheet[y], 1, sizeof sheet[0], f);
    fclose(f);
    printf("date_lab: %d shots -> build/date_lab.ppm", nshots);
    puts("");
    return 0;
}
