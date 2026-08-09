#ifndef MESH_H
#define MESH_H

/* Uctan uca ucgen mesh: tools/glb_to_mesh.py ile uretilen "BWM1" bicimi.
 *
 * Veri EBOOT'a gomulu gelir ve big-endian float/u16 icerir, yani PPU'da
 * ek bir bayt cevrimi gerekmez. Modul yalnizca isaretci hesaplar; kopya
 * yapmaz. */

/* Kendi urettigimiz modelde her hareketli yuzey ayri parca: 27 adet.
 * Sinir 16 iken model yuklenemiyordu. */
#define MESH_MAX_PARTS 40

typedef struct {
    char         name[32];
    unsigned int vertex_count;
    unsigned int index_count;
    const float *verts;         /* 9 float: x,y,z, nx,ny,nz, r,g,b */
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

/* Toplam ucgen sayisi (tani icin) */
unsigned int mesh_triangles(const Mesh *m);

#endif
