/* gamemenu icin host tarafi birim testleri (PS3 gerekmez). */
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
    printf("test: menu kapali basliyor, varsayilanlar makul\n");
    gamemenu_init(&m);
    CHECK(!m.open, "menu kapali");
    CHECK(m.weather == WEATHER_SUNNY, "hava gunesli");
    CHECK(m.time == TIME_DAY, "gunduz");
    CHECK(m.hud_visible, "HUD acik");
    CHECK(!m.quit_requested, "cikis istenmemis");
}

static void test_acma_kapama(void)
{
    GameMenu m;
    printf("test: menu acilip kapaniyor, acilista ilk satir secili\n");
    gamemenu_init(&m);
    m.row = 2;
    gamemenu_toggle(&m);
    CHECK(m.open, "acildi");
    CHECK(m.row == 0, "acilista ilk satira donuldu");
    gamemenu_toggle(&m);
    CHECK(!m.open, "kapandi");
}

static void test_kapaliyken_etkisiz(void)
{
    GameMenu m;
    printf("test: menu kapaliyken girdiler etkisiz\n");
    gamemenu_init(&m);
    gamemenu_move(&m, 1);
    gamemenu_adjust(&m, 1);
    gamemenu_confirm(&m);
    CHECK(m.row == 0, "satir degismedi");
    CHECK(m.weather == WEATHER_SUNNY, "hava degismedi");
    CHECK(!m.quit_requested, "cikis istenmedi");
}

static void test_satir_dolasimi(void)
{
    GameMenu m;
    int i;
    printf("test: satirlar arasi gezinme basa/sona sariyor\n");
    gamemenu_init(&m);
    gamemenu_toggle(&m);

    gamemenu_move(&m, -1);
    CHECK(m.row == ROW_COUNT - 1, "yukari: son satira sardi");

    gamemenu_move(&m, 1);
    CHECK(m.row == 0, "asagi: basa sardi");

    for (i = 0; i < ROW_COUNT; i++)
        gamemenu_move(&m, 1);
    CHECK(m.row == 0, "tam tur atinca basa dondu");
}

static void test_hava_dongusu(void)
{
    GameMenu m;
    int i;
    printf("test: hava secenekleri sirayla donuyor\n");
    gamemenu_init(&m);
    gamemenu_toggle(&m);
    m.row = ROW_WEATHER;

    gamemenu_adjust(&m, 1);
    CHECK(m.weather == WEATHER_CLOUDY, "gunesli -> bulutlu");

    gamemenu_adjust(&m, -1);
    CHECK(m.weather == WEATHER_SUNNY, "geri dondu");

    gamemenu_adjust(&m, -1);
    CHECK(m.weather == WEATHER_COUNT - 1, "basta sola gidince sona sardi");

    for (i = 0; i < WEATHER_COUNT; i++)
        gamemenu_adjust(&m, 1);
    CHECK(m.weather == WEATHER_COUNT - 1, "tam tur ayni yere getirdi");
}

static void test_hud_ac_kapa(void)
{
    GameMenu m;
    printf("test: HUD satiri acip kapiyor\n");
    gamemenu_init(&m);
    gamemenu_toggle(&m);
    m.row = ROW_HUD;

    gamemenu_adjust(&m, 1);
    CHECK(!m.hud_visible, "kapandi");
    gamemenu_adjust(&m, 1);
    CHECK(m.hud_visible, "tekrar acildi");
}

static void test_cikis(void)
{
    GameMenu m;
    printf("test: cikis yalnizca kendi satirinda ve onayla calisiyor\n");
    gamemenu_init(&m);
    gamemenu_toggle(&m);

    m.row = ROW_WEATHER;
    gamemenu_confirm(&m);
    CHECK(!m.quit_requested, "baska satirda onay cikis yapmaz");

    m.row = ROW_QUIT;
    gamemenu_adjust(&m, 1);
    CHECK(!m.quit_requested, "cikis satirinda sag/sol etkisiz");

    gamemenu_confirm(&m);
    CHECK(m.quit_requested, "onay cikis istegi olusturdu");
}

static void test_isimler(void)
{
    int i;
    printf("test: her secenegin bir adi var\n");
    for (i = 0; i < WEATHER_COUNT; i++)
        CHECK(weather_name((Weather)i)[0] != '?', "hava adi tanimli");
    for (i = 0; i < TIME_COUNT; i++)
        CHECK(time_name((TimeOfDay)i)[0] != '?', "gun adi tanimli");
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

    printf("\n%d kontrol, %d hata\n", checks, failures);
    return failures ? 1 : 0;
}
