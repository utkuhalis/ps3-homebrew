#ifndef GAUGES_H
#define GAUGES_H

/* Analog ucus gostergeleri: hiz, suni ufuk, yukseklik.
 *
 * Deger -> ibre acisi donusumu saf matematiktir ve ayri fonksiyonlarda
 * durur; boylece cizimden bagimsiz olarak test edilebilir. */

/* Deger araliklari (gosterge kadranlarinin kapsadigi degerler) */
#define SPEED_MAX_KMH   400.0f
#define ALT_MAX_M      4000.0f

/* Ibre acisi (radyan). Saat 6 yonu baslangictir, saat yonunde artar.
 * Deger araligin disindaysa uclara sabitlenir. */
float gauge_speed_angle(float kmh);
float gauge_alt_angle(float meters);

/* Gostergeleri ekranin sag altina cizer */
void gauges_draw(float speed_kmh, float altitude_m, float pitch_rad,
                 float roll_rad);

/* Suni ufku istenen konumda cizer (onizleme ve test icin acik) */
void gauges_draw_attitude(float cx, float cy, float r, float pitch, float roll);

/* Suni ufuk yardimcilari - saf, birim testli */
float gauge_horizon_dist(float dx, float dy, float roll, float shift);
float gauge_horizon_shift(float pitch, float r);

#endif
