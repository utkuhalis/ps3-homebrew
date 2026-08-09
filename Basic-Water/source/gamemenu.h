#ifndef GAMEMENU_H
#define GAMEMENU_H

/* Oyun ici ayar menusu.
 *
 * Durum mantigi donanimdan bagimsizdir (birim testli); cizim ayri bir
 * fonksiyondadir. Menu acikken sahne arkada akmaya devam eder. */

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
    int       row;          /* secili satir */
    Weather   weather;
    TimeOfDay time;
    int       hud_visible;
    int       show_profiler;
    int       quit_requested;
} GameMenu;

void gamemenu_init(GameMenu *m);

/* Menuyu ac/kapa */
void gamemenu_toggle(GameMenu *m);

/* dir: -1 yukari, +1 asagi. Menu kapaliyken etkisizdir. */
void gamemenu_move(GameMenu *m, int dir);

/* dir: -1 sol, +1 sag. Secili satirin degerini degistirir; "Cikis"
 * satirinda etkisizdir (orada onay tusu kullanilir). */
void gamemenu_adjust(GameMenu *m, int dir);

/* Onay tusu: yalnizca "Cikis" satirinda anlamlidir. */
void gamemenu_confirm(GameMenu *m);

/* Ekrana cizer (menu kapaliysa hicbir sey yapmaz) */
void gamemenu_draw(const GameMenu *m);

/* Gosterim adlari - cizim ve testler ayni metni kullansin diye */
const char *weather_name(Weather w);
const char *time_name(TimeOfDay t);

#endif
