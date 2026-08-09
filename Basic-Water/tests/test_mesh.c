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
        const char *need[] = { "Air Plane", "flap_left", "flap_right",
                               "aileron_left", "aileron_right", "rudder" };
        size_t n;

        for (n = 0; n < sizeof(need) / sizeof(need[0]); n++) {
            if (mesh_find(&m, need[n]) < 0) {
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
        printf("\n4 kontrol, 0 hata\n");
    else
        printf("\n%d HATA\n", failures);
    return failures ? 1 : 0;
}
