#ifndef MENU_H
#define MENU_H

/* Menu kendi ic durumunu (hangi ekran, hangi satir secili) tutar;
 * disariya sadece bir aksiyon bildirir. */

typedef enum {
    MENU_NONE = 0,
    MENU_START_BOT,
    MENU_START_2P,
    MENU_QUIT
} MenuAction;

void       menu_init(void);
MenuAction menu_update(void);
void       menu_draw(void);

#endif
