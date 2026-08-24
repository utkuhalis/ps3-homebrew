#ifndef WORLD_H
#define WORLD_H

#include "camera.h"
#include "mat4.h"
#include "atmosphere.h"

/* Dunya (sahne) katmani.
 *
 * Oyun tek bir sahneye bagli degil: her dunya kendi init/draw islevini
 * verir, ana dongu yalnizca "o an secili olani" cizer. Boylece yeni bir
 * ortam eklemek mevcut olani hic ellemeden yapilabilir - kotu giderse
 * dunyayi listeden cikarmak yeterli.
 *
 * Ucus modeli, kamera, HUD ve ses dunyadan bagimsizdir; hepsi her sahnede
 * ayni calisir. Dunya yalnizca ORTAMI tanimlar: zemin, gokyuzu, pistler,
 * yapilar. */

typedef struct {
    const char *name;

    /* Bir kez, program baslarken. 0: basarili */
    int  (*init)(void);

    /* Sahneyi cizer (3D gecis). Overlay ayri katmandir. */
    void (*draw)(const Camera *cam, const Mat4 *proj, float time_sec,
                 const Atmosphere *atm);

    /* Ucagin baslayacagi pist merkezi, yonu ve pist basina kaydirma */
    void (*start_point)(float out_xz[2], float *heading, float *offset);

    /* Yer temasinin gecerli oldugu dikdortgen (yari olculer) */
    void (*ground_extent)(float *half_len, float *half_wid);
} World;

/* Kayitli dunya sayisi ve erisim */
int          world_count(void);
const World *world_get(int index);

/* O an secili dunya */
int          world_current_index(void);
void         world_select(int index);
const World *world_current(void);

/* Tum dunyalari baslatir; ilki secili olur. 0: basarili */
int          world_init_all(void);

#endif
