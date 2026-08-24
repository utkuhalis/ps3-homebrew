#ifndef SKYCOLOR_H
#define SKYCOLOR_H

/* Gokyuzu rengi: bakis yonune gore gradyan + gunes + bulutlar.
 * Texture kullanilmaz, her sey hesaplanir.
 *
 * ONEMLI: Buradaki formullerin birebir ayni'si shaders/sky.fcg ve
 * shaders/water.fcg icinde de bulunur (Cg'de include yok). Biri
 * degistiginde digerleri de guncellenmelidir; aksi halde suyun yansittigi
 * gokyuzu ile gercek gokyuzu birbirini tutmaz.
 *
 * Bu dosya donanim bagimsizdir: birim testlerde ve host onizlemesinde
 * dogrudan kullanilir. */

/* Gunes yonu (birim vektor) ve rengi - iki shader ile ayni tutulmali */
extern const float SUN_DIR[3];

/* dir: birim bakis vektoru, time: saniye. out: 0..1 araliginda RGB */
void sky_color(const float dir[3], float time, float out[3]);

/* Sadece bulut yogunlugu (0..1) - test ve ayar icin ayrica acildi */
float sky_clouds(const float dir[3], float time);

#endif
