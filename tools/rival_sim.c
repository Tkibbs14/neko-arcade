/* Rival! playtest simulator: the real game code (included below) driven by a bot on a digital D-pad.
 *   rival_sim [matches]
 * 1. Paddle feel: frames to full speed, top speed, stopping distance after release.
 * 2. Matches bot vs Mako's AI through the real update loop: scores, match length, hits, rallies, puck speed and
 *    any stuck puck (slow for 3+ seconds). Built by tools/rivalsim.sh. */
#include <stdio.h>
#include <stdlib.h>
#include "../src/demo_rival.c"

u32 time_us(void) { return 0; }
void platform_log(const char *line) { (void)line; }

/* The bot plays like a person with a D-pad: eight directions, boost when far from where it wants to be, and it
 * reacts to where the puck was a moment ago (casual 10 frames, decent 6, sharp 3). */
static int hist_x[16], hist_y[16], hist_i;

static u16 bot_buttons(int skill)
{
    static const int delay[3] = { 10, 6, 3 };
    hist_x[hist_i & 15] = puck.x;
    hist_y[hist_i & 15] = puck.y;
    int seen = (hist_i - delay[skill]) & 15;
    hist_i++;
    int px = hist_x[seen] / FP, py = hist_y[seen] / FP, ux = you.x / FP, uy = you.y / FP;
    int tx, ty;
    if (px < TMIDX && (puck.vx < 2 * FP || px < ux)) {
        tx = px - 11;                                   /* get behind the puck and drive it toward her goal */
        ty = py + (py < TMIDY ? -2 : 2) * (skill > 1);
        if (px < ux + 4) { tx = px - 16; ty = py + (py < uy ? 14 : -14); }   /* puck behind us: go around it */
    } else {
        tx = TX0 + 26;                                  /* defend in front of the goal */
        ty = iclamp(py, TMIDY - GOAL_H / 2 - 6, TMIDY + GOAL_H / 2 + 6);
    }
    int dead = skill > 1 ? 2 : 4;
    u16 b = 0;
    if (tx < ux - dead) b |= BTN_LEFT;
    if (tx > ux + dead) b |= BTN_RIGHT;
    if (ty < uy - dead) b |= BTN_UP;
    if (ty > uy + dead) b |= BTN_DOWN;
    if (iabs(tx - ux) + iabs(ty - uy) > 30) b |= BTN_B;
    if (skill == 0 && rnd_range(0, 5) == 0) b = 0;      /* a slow reaction now and then */
    return b;
}

static void feel(void)
{
    you = (Body){ (TX0 + 20) * FP, TMIDY * FP, 0, 0 };
    int f90 = -1, top = 0;
    for (int f = 1; f <= 30; f++) {
        move_paddle(&you, 1, FP, 0, SPEED_PLAYER);
        if (you.vx > top) top = you.vx;
        if (f90 < 0 && you.vx >= SPEED_PLAYER * 9 / 10) f90 = f;
    }
    you = (Body){ (TX0 + 20) * FP, TMIDY * FP, SPEED_PLAYER, 0 };
    i32 x0 = you.x;
    int stop = 0;
    while (you.vx && stop < 60) { move_paddle(&you, 1, 0, 0, SPEED_PLAYER); stop++; }
    printf("feel: top speed %.2f px/frame (boost %.2f), 90%% of it after %d frames, stops in %d frames / %.1f px\n",
           top / 256.0, SPEED_BOOST / 256.0, f90, stop, (you.x - x0) / 256.0);
}

int main(int argc, char **argv)
{
    int matches = argc > 1 ? atoi(argv[1]) : 40;
    feel();
    for (int h = 0; h < 2; h++)
    for (int skill = 0; skill < 3; skill++) {
        hard = h;
        if (skill == 0) printf("%s:\n", h ? "Rival difficulty" : "Chill difficulty");
        int bot_wins = 0, frames_total = 0, goals_bot = 0, goals_mako = 0, stuck = 0, hits = 0, max_sp = 0;
        long rally_frames = 0, rallies = 0;
        for (int m = 0; m < matches; m++) {
            save.rival_shots[0] = save.rival_shots[1] = save.rival_shots[2] = 0;
            mode_2p = 0;
            start_match();
            int f = 0, slow = 0, last_hit = -1, since_goal = 0;
            while (state != ST_OVER && f < 60 * 60 * 10) {
                input_update(bot_buttons(skill), 0);
                int before[2] = { score[0], score[1] };
                int lh = last_hitter;
                rival_update();
                f++;
                since_goal++;
                if (last_hitter != lh && last_hitter >= 0) hits++;
                (void)last_hit;
                int sp = (int)isqrt((u32)((puck.vx >> 4) * (puck.vx >> 4) + (puck.vy >> 4) * (puck.vy >> 4))) * 16;
                if (sp > max_sp) max_sp = sp;
                if (state == ST_PLAY && sp < FP / 4) {
                    if (++slow == 180) {
                        stuck++;
                        if (stuck <= 6)
                            printf("  stuck: puck (%d,%d)  you (%d,%d)  Mako (%d,%d)  score %d-%d\n", puck.x / FP, puck.y / FP,
                                   you.x / FP, you.y / FP, her.x / FP, her.y / FP, score[0], score[1]);
                    }
                } else slow = 0;
                if (score[0] != before[0] || score[1] != before[1]) { rally_frames += since_goal; rallies++; since_goal = 0; }
                frame_count++;
            }
            frames_total += f;
            goals_bot += score[0];
            goals_mako += score[1];
            bot_wins += score[0] > score[1];
        }
        static const char *const names[3] = { "casual", "decent", "sharp" };
        printf("%-6s bot vs Mako, %d matches: bot won %d (%d%%), goals %d-%d, match %.0f s, %.1f s per goal, "
               "%.1f hits per goal, top puck speed %.1f px/frame, stuck puck %d times\n",
               names[skill], matches, bot_wins, 100 * bot_wins / matches, goals_bot, goals_mako,
               frames_total / 60.0 / matches, rally_frames / 60.0 / (rallies ? rallies : 1),
               (double)hits / (rallies ? rallies : 1), max_sp / 256.0, stuck);
    }
    return 0;
}
