#ifndef DRAW_H
#define DRAW_H

#include "game.h"

/* Oyun sahasinin cizimi. game.c saf mantik olarak kalsin diye ayri modul. */

void draw_game(const Game *g);
void draw_result(const Game *g, int p1_won);

#endif
