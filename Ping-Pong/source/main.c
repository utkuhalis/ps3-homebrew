#include <stdio.h>
#include <stdlib.h>

#include <ppu-lv2.h>
#include <sys/process.h>
#include <sysutil/sysutil.h>

#include "video.h"
#include "font.h"
#include "input.h"
#include "menu.h"
#include "game.h"
#include "draw.h"

SYS_PROCESS_PARAM(1001, 0x100000)

typedef enum { ST_MENU = 0, ST_GAME, ST_RESULT } AppState;

static int should_exit = 0;

/* XMB'den "oyundan cik" secilirse dongu temiz kapansin */
static void sys_callback(u64 status, u64 param, void *userdata)
{
    (void)param;
    (void)userdata;
    if (status == SYSUTIL_EXIT_GAME)
        should_exit = 1;
}

int main(int argc, const char *argv[])
{
    AppState state = ST_MENU;
    Game game;
    int p1_won = 0;
    unsigned int seed = 1u;
    int rc;

    (void)argc;
    (void)argv;

    rc = video_init();
    if (rc < 0) {
        printf("HATA: video_init basarisiz (%d)\n", rc);
        return 1;
    }

    if (input_init() != 0) {
        printf("HATA: input_init basarisiz\n");
        video_exit();
        return 1;
    }

    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sys_callback, NULL);
    menu_init();

    while (!should_exit) {
        sysUtilCheckCallback();
        input_update();
        seed = seed * 1664525u + 1013904223u;   /* menude gecen sure -> tohum */

        switch (state) {
        case ST_MENU: {
            MenuAction act = menu_update();

            if (act == MENU_QUIT) {
                should_exit = 1;
            } else if (act == MENU_START_BOT || act == MENU_START_2P) {
                game_init(&game,
                          (act == MENU_START_BOT) ? MODE_BOT : MODE_2P,
                          seed);
                state = ST_GAME;
            }
            menu_draw();
            break;
        }

        case ST_GAME: {
            GameStatus st;

            /* Duraklatma: START ana tus; SELECT ve UCGEN de kabul edilir
             * (bazi kurulumlarda START'a erisim zor olabiliyor). */
            if (input_any_pressed(PAD_START | PAD_SELECT | PAD_TRIANGLE))
                game_toggle_pause(&game);

            if (game.paused && input_any_pressed(PAD_CIRCLE)) {
                menu_init();
                state = ST_MENU;
                menu_draw();
                break;
            }

            st = game_update(&game, input_move(0), input_move(1));
            if (st != GAME_RUNNING) {
                p1_won = (st == GAME_P1_WON);
                state = ST_RESULT;
            }
            draw_game(&game);
            break;
        }

        case ST_RESULT:
            if (input_any_pressed(PAD_CROSS) || input_any_pressed(PAD_CIRCLE)) {
                menu_init();
                state = ST_MENU;
            }
            draw_result(&game, p1_won);
            break;
        }

        video_flip();
    }

    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
    input_exit();
    video_exit();

    return 0;
}
