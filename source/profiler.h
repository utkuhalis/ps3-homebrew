#ifndef PROFILER_H
#define PROFILER_H

/* Kare suresi olcumu.
 *
 * Neyin pahali oldugunu tahmin etmek yerine olcmek icin. Her bolum
 * mikrosaniye cinsinden olculur ve son karelerin ortalamasi alinir;
 * tek kare gurultusu boylece kaybolur.
 *
 * Olcum donanim sayaci degil, sistem saatidir (sysGetSystemTime):
 * GPU'nun ne kadar calistigini dogrudan gostermez. Ama RSX'in geride
 * kalmasi FLIP bolumunde bekleme olarak gorunur - yani FLIP buyukse
 * darbogaz GPU'dadir, digerleri buyukse CPU'dadir. */

typedef enum {
    PROF_FLIGHT = 0,    /* ucus modeli + otopilot + ses guncelleme */
    PROF_MODEL,         /* ucak vertexlerinin CPU'da donusturulmesi */
    PROF_SCENE,         /* gokyuzu + su cizim komutlari */
    PROF_RUNWAY,        /* pistler */
    PROF_AIRCRAFT,      /* ucak cizimi */
    PROF_OVERLAY,       /* HUD toplama ve gonderme */
    PROF_FLIP,          /* kare sonu: RSX'i bekleme ve goruntu degistirme */
    PROF_COUNT
} ProfSection;

/* Kare basi */
void  prof_frame_begin(void);

/* Bir bolumun basi ve sonu */
void  prof_begin(ProfSection s);
void  prof_end(ProfSection s);

/* Kare sonu: ortalamalar guncellenir */
void  prof_frame_end(void);

/* Ortalama degerler (mikrosaniye) */
float prof_avg_us(ProfSection s);
float prof_frame_us(void);
float prof_fps(void);

/* Bolum adi (HUD icin) */
const char *prof_name(ProfSection s);

/* Cizim yuku sayaclari - her kare elle bildirilir */
void  prof_set_counts(unsigned int triangles, unsigned int overlay_rects);
unsigned int prof_triangles(void);
unsigned int prof_rects(void);

#endif
