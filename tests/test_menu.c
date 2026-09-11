/* Host-side unit tests for gamemenu (no PS3 needed). */
#include <stdio.h>
#include <string.h>
#include "../source/gamemenu.h"

static int failures = 0, checks = 0;

#define CHECK(cond, msg) do {                                        \
        checks++;                                                    \
        if (!(cond)) {                                               \
            printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            failures++;                                              \
        }                                                            \
    } while (0)

static void test_baslangic(void)
{
    GameMenu m;
    printf("test: the menu starts closed with sensible defaults\n");
    gamemenu_init(&m);
    CHECK(!m.open, "menu closed");
    CHECK(m.weather == WEATHER_SUNNY, "weather sunny");
    CHECK(m.time == TIME_DAY, "gunduz");
    CHECK(m.hud_visible, "HUD on");
    CHECK(!m.quit_requested, "exit not requested");
}

static void test_acma_kapama(void)
{
    GameMenu m;
    printf("test: the menu opens and closes, first row selected on open\n");
    gamemenu_init(&m);
    m.row = 2;
    gamemenu_toggle(&m);
    CHECK(m.open, "acildi");
    CHECK(m.row == 0, "back to the first row on open");
    gamemenu_toggle(&m);
    CHECK(!m.open, "kapandi");
}

static void test_kapaliyken_etkisiz(void)
{
    GameMenu m;
    printf("test: inputs have no effect while the menu is closed\n");
    gamemenu_init(&m);
    gamemenu_move(&m, 1);
    gamemenu_adjust(&m, 1);
    gamemenu_confirm(&m);
    CHECK(m.row == 0, "row unchanged");
    CHECK(m.weather == WEATHER_SUNNY, "weather unchanged");
    CHECK(!m.quit_requested, "exit not requested");
}

static void test_satir_dolasimi(void)
{
    GameMenu m;
    int i;
    printf("test: moving between rows wraps around\n");
    gamemenu_init(&m);
    gamemenu_toggle(&m);

    gamemenu_move(&m, -1);
    CHECK(m.row == ROW_COUNT - 1, "up: wrapped to the last row");

    gamemenu_move(&m, 1);
    CHECK(m.row == 0, "down: wrapped to the start");

    for (i = 0; i < ROW_COUNT; i++)
        gamemenu_move(&m, 1);
    CHECK(m.row == 0, "a full lap returns to the start");
}

static void test_hava_dongusu(void)
{
    GameMenu m;
    int i;
    printf("test: weather options cycle in order\n");
    gamemenu_init(&m);
    gamemenu_toggle(&m);
    m.row = ROW_WEATHER;

    gamemenu_adjust(&m, 1);
    CHECK(m.weather == WEATHER_CLOUDY, "sunny -> cloudy");

    gamemenu_adjust(&m, -1);
    CHECK(m.weather == WEATHER_SUNNY, "went back");

    gamemenu_adjust(&m, -1);
    CHECK(m.weather == WEATHER_COUNT - 1, "left at the start wraps to the end");

    for (i = 0; i < WEATHER_COUNT; i++)
        gamemenu_adjust(&m, 1);
    CHECK(m.weather == WEATHER_COUNT - 1, "a full lap lands in the same place");
}

static void test_hud_ac_kapa(void)
{
    GameMenu m;
    printf("test: the HUD row toggles on and off\n");
    gamemenu_init(&m);
    gamemenu_toggle(&m);
    m.row = ROW_HUD;

    gamemenu_adjust(&m, 1);
    CHECK(!m.hud_visible, "kapandi");
    gamemenu_adjust(&m, 1);
    CHECK(m.hud_visible, "turned on again");
}

static void test_cikis(void)
{
    GameMenu m;
    printf("test: exit works only on its own row and with confirm\n");
    gamemenu_init(&m);
    gamemenu_toggle(&m);

    m.row = ROW_WEATHER;
    gamemenu_confirm(&m);
    CHECK(!m.quit_requested, "confirm on another row does not exit");

    m.row = ROW_QUIT;
    gamemenu_adjust(&m, 1);
    CHECK(!m.quit_requested, "left/right have no effect on the exit row");

    gamemenu_confirm(&m);
    CHECK(m.quit_requested, "confirm created an exit request");
}

static void test_isimler(void)
{
    int i;
    printf("test: every option has a name\n");
    for (i = 0; i < WEATHER_COUNT; i++)
        CHECK(weather_name((Weather)i)[0] != '?', "weather name defined");
    for (i = 0; i < TIME_COUNT; i++)
        CHECK(time_name((TimeOfDay)i)[0] != '?', "time name defined");
}

int main(void)
{
    test_baslangic();
    test_acma_kapama();
    test_kapaliyken_etkisiz();
    test_satir_dolasimi();
    test_hava_dongusu();
    test_hud_ac_kapa();
    test_cikis();
    test_isimler();

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
