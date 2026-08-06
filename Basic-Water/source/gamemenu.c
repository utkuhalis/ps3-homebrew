#include "gamemenu.h"
#include "overlay.h"
#include "font.h"

#define PANEL_X   360
#define PANEL_Y   180
#define PANEL_W   560
#define PANEL_H   300

#define COL_PANEL   RGB(6, 14, 26)
#define COL_BORDER  RGB(120, 190, 255)
#define COL_TITLE   RGB(150, 210, 255)
#define COL_TEXT    RGB(205, 220, 235)
#define COL_SEL     RGB(255, 210, 70)
#define COL_HINT    RGB(120, 140, 160)

static const char *ROW_LABEL[ROW_COUNT] = {
    "Hava", "Gün", "HUD", "Çıkış"
};

const char *weather_name(Weather w)
{
    switch (w) {
    case WEATHER_SUNNY:  return "Güneşli";
    case WEATHER_CLOUDY: return "Bulutlu";
    case WEATHER_RAINY:  return "Yağmurlu";
    case WEATHER_FOGGY:  return "Sisli";
    case WEATHER_STORMY: return "Fırtınalı";
    default:             return "?";
    }
}

const char *time_name(TimeOfDay t)
{
    switch (t) {
    case TIME_DAY:    return "Gündüz";
    case TIME_SUNSET: return "Gün Batımı";
    case TIME_NIGHT:  return "Gece";
    default:          return "?";
    }
}

void gamemenu_init(GameMenu *m)
{
    m->open = 0;
    m->row = 0;
    m->weather = WEATHER_SUNNY;
    m->time = TIME_DAY;
    m->hud_visible = 1;
    m->quit_requested = 0;
}

void gamemenu_toggle(GameMenu *m)
{
    m->open = !m->open;
    if (m->open)
        m->row = 0;
}

static int wrap(int v, int count)
{
    if (v < 0)
        return count - 1;
    if (v >= count)
        return 0;
    return v;
}

void gamemenu_move(GameMenu *m, int dir)
{
    if (!m->open || dir == 0)
        return;
    m->row = wrap(m->row + dir, ROW_COUNT);
}

void gamemenu_adjust(GameMenu *m, int dir)
{
    if (!m->open || dir == 0)
        return;

    switch (m->row) {
    case ROW_WEATHER:
        m->weather = (Weather)wrap((int)m->weather + dir, WEATHER_COUNT);
        break;
    case ROW_TIME:
        m->time = (TimeOfDay)wrap((int)m->time + dir, TIME_COUNT);
        break;
    case ROW_HUD:
        m->hud_visible = !m->hud_visible;
        break;
    default:
        break;              /* Cikis satirinda deger yok */
    }
}

void gamemenu_confirm(GameMenu *m)
{
    if (!m->open)
        return;
    if (m->row == ROW_QUIT)
        m->quit_requested = 1;
}

static const char *row_value(const GameMenu *m, int row)
{
    switch (row) {
    case ROW_WEATHER: return weather_name(m->weather);
    case ROW_TIME:    return time_name(m->time);
    case ROW_HUD:     return m->hud_visible ? "Açık" : "Kapalı";
    default:          return "";
    }
}

void gamemenu_draw(const GameMenu *m)
{
    int i;

    if (!m->open)
        return;

    /* yari saydam panel + ince cerceve */
    overlay_blend_rect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COL_PANEL, 205);
    overlay_fill_rect(PANEL_X, PANEL_Y, PANEL_W, 2, COL_BORDER);
    overlay_fill_rect(PANEL_X, PANEL_Y + PANEL_H - 2, PANEL_W, 2, COL_BORDER);
    overlay_fill_rect(PANEL_X, PANEL_Y, 2, PANEL_H, COL_BORDER);
    overlay_fill_rect(PANEL_X + PANEL_W - 2, PANEL_Y, 2, PANEL_H, COL_BORDER);

    font_draw_center(PANEL_X + PANEL_W / 2, PANEL_Y + 26, 3, "AYARLAR", COL_TITLE);

    for (i = 0; i < ROW_COUNT; i++) {
        int y = PANEL_Y + 90 + i * 46;
        color_t c = (i == m->row) ? COL_SEL : COL_TEXT;

        if (i == m->row)
            font_draw_text(PANEL_X + 30, y, 2, ">", c);

        font_draw_text(PANEL_X + 60, y, 2, ROW_LABEL[i], c);

        if (i != ROW_QUIT) {
            font_draw_text(PANEL_X + 250, y, 2, row_value(m, i), c);

            if (i == m->row) {
                font_draw_text(PANEL_X + 230, y, 2, "<", COL_HINT);
                font_draw_text(PANEL_X + 500, y, 2, ">", COL_HINT);
            }
        }
    }

    font_draw_center(PANEL_X + PANEL_W / 2, PANEL_Y + PANEL_H - 34, 1,
                     "Yön: seç   Sol/Sağ: değiştir   X: onayla   SELECT: kapat",
                     COL_HINT);
}
