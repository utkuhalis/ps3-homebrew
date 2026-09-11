#ifndef GAMEMENU_H
#define GAMEMENU_H

/* In-game settings menu.
 *
 * The state logic is hardware independent (unit tested); drawing lives in
 * a separate function. The scene keeps running behind the open menu. */

typedef enum {
    WEATHER_SUNNY = 0,
    WEATHER_CLOUDY,
    WEATHER_RAINY,
    WEATHER_FOGGY,
    WEATHER_STORMY,
    WEATHER_COUNT
} Weather;

typedef enum {
    TIME_DAY = 0,
    TIME_SUNSET,
    TIME_NIGHT,
    TIME_COUNT
} TimeOfDay;

typedef enum {
    ROW_WEATHER = 0,
    ROW_TIME,
    ROW_HUD,
    ROW_PROFILER,
    ROW_QUIT,
    ROW_COUNT
} MenuRow;

typedef struct {
    int       open;
    int       row;          /* selected row */
    Weather   weather;
    TimeOfDay time;
    int       hud_visible;
    int       show_profiler;
    int       quit_requested;
} GameMenu;

void gamemenu_init(GameMenu *m);

/* Open/close the menu */
void gamemenu_toggle(GameMenu *m);

/* dir: -1 up, +1 down. No effect while the menu is closed. */
void gamemenu_move(GameMenu *m, int dir);

/* dir: -1 left, +1 right. Changes the value of the selected row; no effect
 * on the "Exit" row (the confirm button is used there). */
void gamemenu_adjust(GameMenu *m, int dir);

/* Confirm button: only meaningful on the "Exit" row. */
void gamemenu_confirm(GameMenu *m);

/* Draws to the screen (does nothing while the menu is closed) */
void gamemenu_draw(const GameMenu *m);

/* Display names - so drawing and tests use the same text */
const char *weather_name(Weather w);
const char *time_name(TimeOfDay t);

#endif
