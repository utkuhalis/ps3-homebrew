/* game.c icin host tarafi birim testleri (PS3 donanimi gerekmez).
 * Calistirma: tests/run_tests.sh  */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../source/game.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond, msg) do {                                   \
        checks++;                                               \
        if (!(cond)) {                                          \
            printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            failures++;                                         \
        }                                                       \
    } while (0)

static void test_ust_duvardan_sekme(void)
{
    Game g;
    float vy_once;

    printf("test: ust duvardan sekme\n");
    game_init(&g, MODE_2P, 42);
    g.bx = FIELD_W / 2.0f;
    g.by = 5.0f;
    g.vx = 0.0f;
    g.vy = -8.0f;
    vy_once = g.vy;

    game_update(&g, 0, 0);

    CHECK(g.by >= 0.0f, "top ust duvarin disina cikmadi");
    CHECK(g.vy == -vy_once, "dikey hiz isaret degistirdi");
}

static void test_alt_duvardan_sekme(void)
{
    Game g;

    printf("test: alt duvardan sekme\n");
    game_init(&g, MODE_2P, 7);
    g.bx = FIELD_W / 2.0f;
    g.by = FIELD_H - BALL_SIZE - 3.0f;
    g.vx = 0.0f;
    g.vy = 8.0f;

    game_update(&g, 0, 0);

    CHECK(g.by + BALL_SIZE <= FIELD_H, "top alt duvarin disina cikmadi");
    CHECK(g.vy < 0.0f, "dikey hiz yukari dondu");
}

static void test_raket_vurusu(void)
{
    Game g;
    float hiz_once;

    printf("test: raket vurusu topu geri sektiriyor ve hizlandiriyor\n");
    game_init(&g, MODE_2P, 99);
    g.p1y = FIELD_H / 2.0f - PADDLE_H / 2.0f;
    g.by = FIELD_H / 2.0f - BALL_SIZE / 2.0f;   /* raketin tam ortasi */
    g.bx = PADDLE_MARGIN + PADDLE_W - 2.0f;
    g.vx = -8.0f;
    g.vy = 0.0f;
    g.speed = 8.0f;
    hiz_once = g.speed;

    game_update(&g, 0, 0);

    CHECK(g.vx > 0.0f, "top saga dondu");
    CHECK(g.speed > hiz_once, "vurusta hiz artti");
    CHECK(g.bx >= PADDLE_MARGIN + PADDLE_W - 0.01f, "top raketin icinde kalmadi");
}

static void test_vurus_acisi_carpma_noktasina_bagli(void)
{
    Game g1, g2;

    printf("test: raketin ucuna carpan top daha genis aci aliyor\n");

    /* merkeze carpma */
    game_init(&g1, MODE_2P, 5);
    g1.p1y = FIELD_H / 2.0f - PADDLE_H / 2.0f;
    g1.by = FIELD_H / 2.0f - BALL_SIZE / 2.0f;
    g1.bx = PADDLE_MARGIN + PADDLE_W - 2.0f;
    g1.vx = -8.0f; g1.vy = 0.0f; g1.speed = 8.0f;
    game_update(&g1, 0, 0);

    /* ust uca carpma */
    game_init(&g2, MODE_2P, 5);
    g2.p1y = FIELD_H / 2.0f - PADDLE_H / 2.0f;
    g2.by = g2.p1y - BALL_SIZE + 4.0f;
    g2.bx = PADDLE_MARGIN + PADDLE_W - 2.0f;
    g2.vx = -8.0f; g2.vy = 0.0f; g2.speed = 8.0f;
    game_update(&g2, 0, 0);

    CHECK(fabsf(g1.vy) < 1.0f, "merkez vurusu neredeyse duz gidiyor");
    CHECK(fabsf(g2.vy) > fabsf(g1.vy), "uc vurusu daha dik aci veriyor");
    CHECK(g2.vy < 0.0f, "ust uca carpan top yukari gidiyor");
}

static void test_sayi_ve_servis(void)
{
    Game g;

    printf("test: soldan cikan top rakibe sayi yaziyor\n");
    game_init(&g, MODE_2P, 3);
    g.bx = -BALL_SIZE - 5.0f;
    g.by = FIELD_H / 2.0f;
    g.vx = -8.0f;
    g.vy = 0.0f;

    game_update(&g, 0, 0);

    CHECK(g.score2 == 1, "sag oyuncu 1 sayi aldi");
    CHECK(g.score1 == 0, "sol oyuncunun sayisi degismedi");
    CHECK(fabsf(g.bx - (FIELD_W / 2.0f - BALL_SIZE / 2.0f)) < 1.0f,
          "top merkeze donmus");
    CHECK(g.vx < 0.0f, "servis sayiyi yiyen tarafa dogru");
}

static void test_mac_sonu(void)
{
    Game g;
    GameStatus st;

    printf("test: 11 sayiya ulasan kazaniyor\n");
    game_init(&g, MODE_2P, 11);
    g.score1 = WIN_SCORE - 1;
    g.bx = FIELD_W + 5.0f;
    g.vx = 8.0f;
    g.vy = 0.0f;

    st = game_update(&g, 0, 0);

    CHECK(st == GAME_P1_WON, "sol oyuncu maci kazandi");
    CHECK(g.score1 == WIN_SCORE, "skor 11 oldu");
}

static void test_raket_sinirlari(void)
{
    Game g;
    int i;

    printf("test: raketler sahanin disina tasmiyor\n");
    game_init(&g, MODE_2P, 21);

    for (i = 0; i < 300; i++)
        game_update(&g, -1, 1);

    CHECK(g.p1y >= 0.0f, "sol raket ust sinirda durdu");
    CHECK(g.p2y + PADDLE_H <= FIELD_H, "sag raket alt sinirda durdu");
}

static void test_orantili_hareket(void)
{
    Game tam, yarim;
    float tam_mesafe, yarim_mesafe, baslangic;

    printf("test: analog cubugu az itmek raketi yavas hareket ettiriyor\n");
    game_init(&tam, MODE_2P, 4);
    game_init(&yarim, MODE_2P, 4);
    baslangic = tam.p1y;

    game_update(&tam, 1.0f, 0.0f);
    game_update(&yarim, 0.5f, 0.0f);

    tam_mesafe = tam.p1y - baslangic;
    yarim_mesafe = yarim.p1y - baslangic;

    CHECK(tam_mesafe > 0.0f, "tam guc raketi hareket ettirdi");
    CHECK(yarim_mesafe > 0.0f, "yarim guc de hareket ettirdi");
    CHECK(fabsf(yarim_mesafe - tam_mesafe / 2.0f) < 0.01f,
          "yarim guc tam gucun yarisi kadar hareket");
}

static void test_duraklatma(void)
{
    Game g;
    float bx, by;

    printf("test: duraklatilinca hicbir sey hareket etmiyor\n");
    game_init(&g, MODE_2P, 8);
    bx = g.bx; by = g.by;
    game_toggle_pause(&g);

    game_update(&g, 1, 1);

    CHECK(g.bx == bx && g.by == by, "top duraklatmada sabit kaldi");
    CHECK(g.p1y == FIELD_H / 2.0f - PADDLE_H / 2.0f, "raket duraklatmada sabit");
}

static void test_bot_maci_bitiyor_ve_yenilebilir(void)
{
    Game g;
    GameStatus st = GAME_RUNNING;
    long i;
    int bot_disarida = 0;

    printf("test: bot maci makul surede bitiyor, bot sahada kaliyor\n");
    game_init(&g, MODE_BOT, 1234);

    /* Oyuncu hic oynamiyor: bot kazanmali ve mac bir yerde bitmeli. */
    for (i = 0; i < 200000 && st == GAME_RUNNING; i++) {
        st = game_update(&g, 0, 0);
        if (g.p2y < 0.0f || g.p2y + PADDLE_H > FIELD_H)
            bot_disarida = 1;
    }

    CHECK(st != GAME_RUNNING, "mac sonlandi");
    CHECK(st == GAME_P2_WON, "hic oynamayan oyuncuya karsi bot kazandi");
    CHECK(!bot_disarida, "bot raketi hep sahada kaldi");
}

static void test_bot_kusursuz_degil(void)
{
    Game g;
    GameStatus st = GAME_RUNNING;
    long i;

    printf("test: bot orta zorlukta - mukemmel takipci degil\n");
    game_init(&g, MODE_BOT, 2468);

    /* Basit ama iyi bir oyuncu: topu takip et. Botu en az bir kez gecmeli. */
    for (i = 0; i < 400000 && st == GAME_RUNNING; i++) {
        float hedef = g.by + BALL_SIZE / 2.0f - PADDLE_H / 2.0f;
        int dir = (hedef > g.p1y + 2.0f) ? 1 : (hedef < g.p1y - 2.0f ? -1 : 0);
        st = game_update(&g, dir, 0);
    }

    CHECK(st != GAME_RUNNING, "mac sonlandi");
    CHECK(g.score1 > 0, "topu takip eden oyuncu en az bir sayi alabildi");
}

static void test_top_sahayi_terk_etmiyor(void)
{
    Game g;
    long i;
    int dikeyde_tasti = 0;

    printf("test: uzun simulasyonda top dikeyde sahayi terk etmiyor\n");
    game_init(&g, MODE_BOT, 777);

    for (i = 0; i < 200000; i++) {
        game_update(&g, (i / 37) % 3 - 1, 0);
        if (g.by < -0.5f || g.by + BALL_SIZE > FIELD_H + 0.5f)
            dikeyde_tasti = 1;
    }

    CHECK(!dikeyde_tasti, "top hicbir karede ust/alt sinirin disina cikmadi");
}

int main(void)
{
    test_ust_duvardan_sekme();
    test_alt_duvardan_sekme();
    test_raket_vurusu();
    test_vurus_acisi_carpma_noktasina_bagli();
    test_sayi_ve_servis();
    test_mac_sonu();
    test_raket_sinirlari();
    test_orantili_hareket();
    test_duraklatma();
    test_bot_maci_bitiyor_ve_yenilebilir();
    test_bot_kusursuz_degil();
    test_top_sahayi_terk_etmiyor();

    printf("\n%d kontrol, %d hata\n", checks, failures);
    return failures ? 1 : 0;
}
