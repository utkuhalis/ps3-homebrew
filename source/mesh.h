#ifndef MESH_H
#define MESH_H

/* Uctan uca ucgen mesh: tools/glb_to_mesh.py ile uretilen "BWM1" bicimi.
 *
 * Veri EBOOT'a gomulu gelir ve big-endian float/u16 icerir, yani PPU'da
 * ek bir bayt cevrimi gerekmez. Modul yalnizca isaretci hesaplar; kopya
 * yapmaz. */

/* Gercek ucak modellerinde her hareketli yuzey ayri parca oluyor: kendi
 * urettigimiz jet 27, 737 varligi 41 parca. Sinir once 16, sonra 40 idi ve
 * her ikisinde de model sessizce reddedildi - mesh testi ikisini de
 * yakaladi. */
#define MESH_MAX_PARTS 64

/* Vertex duzeni: x,y,z, nx,ny,nz, r,g,b, metallic */
#define MESH_VERTEX_FLOATS 10
#define MESH_VERTEX_BYTES  (MESH_VERTEX_FLOATS * 4)

typedef struct {
    char         name[32];
    unsigned int vertex_count;
    unsigned int index_count;
    const float *verts;         /* MESH_VERTEX_FLOATS float */
    const unsigned short *idx;
} MeshPart;

typedef struct {
    int      part_count;
    MeshPart part[MESH_MAX_PARTS];
    /* tum parcalari kapsayan sinir kutusu (yerel uzayda) */
    float    min[3];
    float    max[3];
} Mesh;

/* Gomulu veriyi cozumler. Basarili ise 0, bicim hatasinda -1. */
int mesh_load(Mesh *m, const void *data, unsigned int size);

/* Bir parcanin adiyla aranmasi; bulunamazsa -1 */
int mesh_find(const Mesh *m, const char *name);

/* Onek ile arama: gercek modellerde ayni yuzey birden cok panele bolunmus
 * olabiliyor (flap_left01..04). */
int mesh_find_prefix(const Mesh *m, const char *prefix);

/* Toplam ucgen sayisi (tani icin) */
unsigned int mesh_triangles(const Mesh *m);

#endif
