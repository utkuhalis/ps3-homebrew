#include <math.h>
#include "game.h"

#define PADDLE_SPEED      9.0f
#define BOT_SPEED         7.0f
#define BALL_START_SPEED  8.0f
#define BALL_MAX_SPEED   18.0f
#define SPEED_UP          1.02f
#define MAX_BOUNCE_ANGLE  0.9f   /* radyan, ~52 derece */
#define BOT_REACT_FRAMES  8
#define BOT_ERROR        30.0f   /* botun nisan sapmasi (px) */

static int rng_next(Game *g)
{
    g->rng = g->rng * 1103515245u + 12345u;
    return (int)((g->rng >> 16) & 0x7fff);
}

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static void serve(Game *g, int toward_p1)
{
    float angle;

    g->bx = FIELD_W / 2.0f - BALL_SIZE / 2.0f;
    g->by = FIELD_H / 2.0f - BALL_SIZE / 2.0f;
    g->speed = BALL_START_SPEED;

    /* -0.35 .. +0.35 radyan arasi rastgele dikey acili servis */
    angle = ((rng_next(g) % 701) - 350) / 1000.0f;
    g->vx = (toward_p1 ? -1.0f : 1.0f) * g->speed * cosf(angle);
    g->vy = g->speed * sinf(angle);

    g->bot_target = FIELD_H / 2.0f - PADDLE_H / 2.0f;
    g->bot_delay = BOT_REACT_FRAMES;
}

void game_init(Game *g, GameMode mode, unsigned int seed)
{
    g->mode = mode;
    g->p1y = g->p2y = FIELD_H / 2.0f - PADDLE_H / 2.0f;
    g->score1 = g->score2 = 0;
    g->paused = 0;
    g->rng = seed ? seed : 1u;
    serve(g, rng_next(g) & 1);
}

void game_toggle_pause(Game *g)
{
    g->paused = !g->paused;
}

/* Rakete carpan topu sektirir. dir: +1 saga, -1 sola */
static void bounce(Game *g, float paddle_y, int dir)
{
    float offset, angle;

    offset = ((g->by + BALL_SIZE / 2.0f) - (paddle_y + PADDLE_H / 2.0f))
             / (PADDLE_H / 2.0f);
    offset = clampf(offset, -1.0f, 1.0f);
    angle = offset * MAX_BOUNCE_ANGLE;

    g->speed = clampf(g->speed * SPEED_UP, BALL_START_SPEED, BALL_MAX_SPEED);
    g->vx = dir * g->speed * cosf(angle);
    g->vy = g->speed * sinf(angle);
}

static void update_bot(Game *g)
{
    float d;

    if (--g->bot_delay <= 0) {
        g->bot_delay = BOT_REACT_FRAMES;
        if (g->vx > 0.0f) {
            /* top bota dogru geliyor: takip et, ufak sapmayla */
            g->bot_target = g->by + BALL_SIZE / 2.0f - PADDLE_H / 2.0f
                            + (rng_next(g) % (int)(2 * BOT_ERROR)) - BOT_ERROR;
        } else {
            /* top uzaklasiyor: merkeze don */
            g->bot_target = FIELD_H / 2.0f - PADDLE_H / 2.0f;
        }
    }

    d = g->bot_target - g->p2y;
    d = clampf(d, -BOT_SPEED, BOT_SPEED);
    g->p2y = clampf(g->p2y + d, 0.0f, FIELD_H - PADDLE_H);
}

GameStatus game_update(Game *g, float p1dir, float p2dir)
{
    float p1x2 = PADDLE_MARGIN + PADDLE_W;
    float p2x1 = FIELD_W - PADDLE_MARGIN - PADDLE_W;

    if (g->paused)
        return GAME_RUNNING;

    /* --- raketler --- */
    g->p1y = clampf(g->p1y + p1dir * PADDLE_SPEED, 0.0f, FIELD_H - PADDLE_H);

    if (g->mode == MODE_BOT)
        update_bot(g);
    else
        g->p2y = clampf(g->p2y + p2dir * PADDLE_SPEED, 0.0f, FIELD_H - PADDLE_H);

    /* --- top --- */
    g->bx += g->vx;
    g->by += g->vy;

    /* ust/alt duvar */
    if (g->by < 0.0f) {
        g->by = 0.0f;
        g->vy = -g->vy;
    } else if (g->by + BALL_SIZE > FIELD_H) {
        g->by = FIELD_H - BALL_SIZE;
        g->vy = -g->vy;
    }

    /* sol raket */
    if (g->vx < 0.0f &&
        g->bx < p1x2 && g->bx + BALL_SIZE > (float)PADDLE_MARGIN &&
        g->by + BALL_SIZE > g->p1y && g->by < g->p1y + PADDLE_H) {
        g->bx = p1x2;
        bounce(g, g->p1y, +1);
    }

    /* sag raket */
    if (g->vx > 0.0f &&
        g->bx + BALL_SIZE > p2x1 && g->bx < p2x1 + PADDLE_W &&
        g->by + BALL_SIZE > g->p2y && g->by < g->p2y + PADDLE_H) {
        g->bx = p2x1 - BALL_SIZE;
        bounce(g, g->p2y, -1);
    }

    /* --- sayi --- */
    if (g->bx + BALL_SIZE < 0.0f) {
        g->score2++;
        if (g->score2 >= WIN_SCORE)
            return GAME_P2_WON;
        serve(g, 1);            /* sayiyi yiyen tarafa servis */
    } else if (g->bx > FIELD_W) {
        g->score1++;
        if (g->score1 >= WIN_SCORE)
            return GAME_P1_WON;
        serve(g, 0);
    }

    return GAME_RUNNING;
}
