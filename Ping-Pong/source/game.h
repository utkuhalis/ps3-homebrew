#ifndef GAME_H
#define GAME_H

/* Oyun mantigi: donanimdan tamamen bagimsizdir.
 * Sanal 1280x720 koordinat sisteminde calisir; cizim katmani olcekler. */

#define FIELD_W        1280
#define FIELD_H        720
#define PADDLE_W       20
#define PADDLE_H       120
#define BALL_SIZE      20
#define PADDLE_MARGIN  60
#define WIN_SCORE      11

typedef enum { MODE_BOT = 0, MODE_2P = 1 } GameMode;
typedef enum { GAME_RUNNING = 0, GAME_P1_WON, GAME_P2_WON } GameStatus;

typedef struct {
    GameMode mode;
    float p1y, p2y;        /* raketlerin ust kenari */
    float bx, by;          /* topun sol-ust kosesi */
    float vx, vy;          /* kare basina hiz */
    float speed;           /* toplam hiz buyuklugu */
    int   score1, score2;
    int   paused;
    unsigned int rng;      /* deterministik LCG - test edilebilirlik icin */
    float bot_target;      /* botun nisan aldigi raket ust kenari */
    int   bot_delay;       /* reaksiyon gecikmesi sayaci */
} Game;

/* p1dir/p2dir: -1.0 .. +1.0 (yukari .. asagi).
 * Ara degerler orantili hiz demektir: analog cubugu az itmek raketi yavas
 * hareket ettirir. */
void       game_init(Game *g, GameMode mode, unsigned int seed);
GameStatus game_update(Game *g, float p1dir, float p2dir);
void       game_toggle_pause(Game *g);

#endif
