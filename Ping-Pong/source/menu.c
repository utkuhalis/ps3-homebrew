#include "menu.h"
#include "input.h"
#include "video.h"
#include "font.h"
#include "game.h"   /* FIELD_W / FIELD_H */

#define COL_BG      RGB(10, 20, 40)
#define COL_TEXT    RGB(200, 210, 230)
#define COL_SEL     RGB(255, 210, 60)
#define COL_TITLE   RGB(120, 200, 255)
#define COL_HINT    RGB(110, 120, 140)

typedef enum { SCREEN_MAIN = 0, SCREEN_START, SCREEN_ABOUT } Screen;

static Screen screen;
static int sel_main;
static int sel_start;

static const char *main_items[3]  = { "Başlat", "Hakkında", "Çıkış" };
static const char *start_items[3] = { "Bota Karşı", "2 Kişi", "Geri" };

void menu_init(void)
{
    screen = SCREEN_MAIN;
    sel_main = 0;
    sel_start = 0;
}

static void move_sel(int *sel, int count, int dir)
{
    *sel += dir;
    if (*sel < 0)
        *sel = count - 1;
    else if (*sel >= count)
        *sel = 0;
}

MenuAction menu_update(void)
{
    int dir = input_any_dir_pressed();
    int ok = input_any_pressed(PAD_CROSS);
    int back = input_any_pressed(PAD_CIRCLE);

    switch (screen) {
    case SCREEN_MAIN:
        if (dir != 0)
            move_sel(&sel_main, 3, dir);
        if (ok) {
            if (sel_main == 0) {
                screen = SCREEN_START;
                sel_start = 0;
            } else if (sel_main == 1) {
                screen = SCREEN_ABOUT;
            } else {
                return MENU_QUIT;
            }
        }
        break;

    case SCREEN_START:
        if (dir != 0)
            move_sel(&sel_start, 3, dir);
        if (back) {
            screen = SCREEN_MAIN;
        } else if (ok) {
            if (sel_start == 0)
                return MENU_START_BOT;
            if (sel_start == 1)
                return MENU_START_2P;
            screen = SCREEN_MAIN;
        }
        break;

    case SCREEN_ABOUT:
        if (back || ok)
            screen = SCREEN_MAIN;
        break;
    }

    return MENU_NONE;
}

static void draw_list(const char **items, int count, int sel, int top)
{
    int i;

    for (i = 0; i < count; i++) {
        int y = top + i * 70;
        color_t c = (i == sel) ? COL_SEL : COL_TEXT;

        if (i == sel)
            font_draw_text(FIELD_W / 2 - 220, y, 4, ">", c);
        font_draw_text(FIELD_W / 2 - 160, y, 4, items[i], c);
    }
}

void menu_draw(void)
{
    video_clear(COL_BG);
    font_draw_center(FIELD_W / 2, 90, 8, "PING PONG", COL_TITLE);

    switch (screen) {
    case SCREEN_MAIN:
        draw_list(main_items, 3, sel_main, 280);
        font_draw_center(FIELD_W / 2, 620, 2,
                         "Yön tuşları: seç    X: onayla", COL_HINT);
        break;

    case SCREEN_START:
        draw_list(start_items, 3, sel_start, 280);
        font_draw_center(FIELD_W / 2, 620, 2,
                         "X: onayla    O: geri", COL_HINT);
        break;

    case SCREEN_ABOUT:
        font_draw_center(FIELD_W / 2, 320, 4, "Utku Halis", COL_TEXT);
        font_draw_center(FIELD_W / 2, 390, 4, "PS3 deneme oyunu", COL_TEXT);
        font_draw_center(FIELD_W / 2, 620, 2, "O: geri", COL_HINT);
        break;
    }
}
