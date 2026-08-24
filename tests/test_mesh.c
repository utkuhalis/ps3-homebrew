/* Mesh cozumleyicisinin uretici ile uyumlu oldugunu GERCEK veri dosyasi
 * uzerinde dogrular.
 *
 * Bir kez uretici tarafina 4 bayt hizalama dolgusu eklenip okuyucuya
 * eklenmemisti; dosya bozuk gorundu ve oyun acilmadi. Bu test o sinif
 * hatayi PS3'e gitmeden yakalar. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../source/mesh.h"

/* Mesh verisi big-endian float tasir; PS3'un PPU'su bunu dogrudan okur ama
 * host (x86/arm) little-endian oldugu icin ayni isaretciden okumak cop
 * deger verir. Test tarafinda bayt sirasi cevrilerek okunur. */
static float be_float(const float *p)
{
    unsigned char b[4];
    unsigned char s[4];
    float out;

    memcpy(b, p, 4);
    s[0] = b[3]; s[1] = b[2]; s[2] = b[1]; s[3] = b[0];
    memcpy(&out, s, 4);
    return out;
}

int main(void)
{
    FILE *fp = fopen("data/plane.bin", "rb");
    unsigned char *buf;
    long size;
    Mesh m;
    int failures = 0;
    int i;

    if (fp == NULL) {
        printf("FAIL: data/plane.bin acilamadi\n");
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buf = (unsigned char *)malloc((size_t)size);
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        printf("FAIL: dosya okunamadi\n");
        return 1;
    }
    fclose(fp);

    if (mesh_load(&m, buf, (unsigned int)size) != 0) {
        printf("FAIL: mesh_load basarisiz (uretici/okuyucu uyumsuz)\n");
        return 1;
    }

    /* Metaliklik gecerli araliкta olmali: shader yansimayi bununla
     * olceklendiriyor, bozuk deger tum govdeyi ayna ya da mat yapar. */
    {
        int bad = 0;
        int p;

        for (p = 0; p < m.part_count; p++) {
            unsigned int v;

            for (v = 0; v < m.part[p].vertex_count; v++) {
                float mt = be_float(&m.part[p].verts[v * MESH_VERTEX_FLOATS + 9]);

                if (!(mt >= -0.01f && mt <= 1.01f))
                    bad++;
            }
        }
        if (bad) {
            printf("FAIL: %d vertexte metaliklik araligin disinda\n", bad);
            failures++;
        } else {
            printf("test: metaliklik degerleri gecerli\n");
        }
    }
    printf("test: mesh_load gercek veriyi cozumluyor (%d parca)\n",
           m.part_count);

    if (m.part_count < 6) {
        printf("FAIL: parca sayisi beklenenden az: %d\n", m.part_count);
        failures++;
    }

    /* Cozumleme dogru ise son parcanin sonu dosya sonuna oturmali:
     * offset kaymasi burada ortaya cikar. */
    {
        const unsigned char *last_end =
            (const unsigned char *)m.part[m.part_count - 1].idx
            + m.part[m.part_count - 1].index_count * 2;
        long used = last_end - buf;

        if (used > size || size - used > 4) {
            printf("FAIL: cozumleme dosya sonuna oturmuyor "
                   "(kullanilan %ld, dosya %ld)\n", used, size);
            failures++;
        } else {
            printf("test: parca offsetleri dosya sonuna oturuyor\n");
        }
    }

    /* Kumanda yuzeyleri ayri parca olarak gelmeli - animasyon buna bagli */
    {
        /* Her modelde bulunmasi ZORUNLU parcalar. Elevator ve tekerlek
         * gobegi modele gore degisir: 737 varliginda yatay stabilizator
         * govdeye dahil ve tekerlekler takim grubunun icinde, bizim
         * urettigimiz jette ise ayri parcalar. Bu yuzden burada yalnizca
         * her ucakta olmasi gereken yuzeyler aranir. */
        const char *need[] = { "body", "flap_left", "flap_right",
                               "aileron_left", "aileron_right", "rudder",
                               "spoiler_left", "spoiler_right" };
        size_t n;

        for (n = 0; n < sizeof(need) / sizeof(need[0]); n++) {
            /* Parcalar numarali da olabilir (flap_left01); onek yeter. */
            if (mesh_find_prefix(&m, need[n]) < 0) {
                printf("FAIL: parca eksik: %s\n", need[n]);
                failures++;
            }
        }
        if (failures == 0)
            printf("test: kumanda yuzeyleri ayri parca olarak var\n");
    }

    /* Hicbir parca u16 index sinirini asmamali */
    for (i = 0; i < m.part_count; i++) {
        if (m.part[i].vertex_count > 65535) {
            printf("FAIL: %s u16 index sinirini asiyor\n", m.part[i].name);
            failures++;
        }
    }

    free(buf);

    if (failures == 0)
        printf("\n6 kontrol, 0 hata\n");
    else
        printf("\n%d HATA\n", failures);
    return failures ? 1 : 0;
}
