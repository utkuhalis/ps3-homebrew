#include <string.h>

#include "mesh.h"

/* Big-endian okuyucular. PPU zaten big-endian oldugu icin bu fonksiyonlar
 * dogrudan kopyalar; ayni kod host testinde (little-endian x86/arm) de
 * dogru calissin diye bayt bayt birlestiriliyor. */
static unsigned int be32(const unsigned char *p)
{
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
           ((unsigned int)p[2] << 8) | (unsigned int)p[3];
}

int mesh_load(Mesh *m, const void *data, unsigned int size)
{
    const unsigned char *p = (const unsigned char *)data;
    unsigned int off = 0;
    int i, k;

    memset(m, 0, sizeof(*m));

    if (size < 8 || p[0] != 'B' || p[1] != 'W' || p[2] != 'M' || p[3] != '2')
        return -1;

    m->part_count = (int)be32(p + 4);
    if (m->part_count <= 0 || m->part_count > MESH_MAX_PARTS)
        return -1;

    for (k = 0; k < 3; k++) {
        m->min[k] =  1e30f;
        m->max[k] = -1e30f;
    }

    off = 8;
    for (i = 0; i < m->part_count; i++) {
        MeshPart *mp = &m->part[i];
        unsigned int need;

        if (off + 40 > size)
            return -1;

        memcpy(mp->name, p + off, 31);
        mp->name[31] = '\0';
        off += 32;

        mp->vertex_count = be32(p + off);
        mp->index_count  = be32(p + off + 4);
        off += 8;

        need = mp->vertex_count * MESH_VERTEX_BYTES + mp->index_count * 2;
        if (off + need > size)
            return -1;

        mp->verts = (const float *)(const void *)(p + off);
        off += mp->vertex_count * MESH_VERTEX_BYTES;
        mp->idx = (const unsigned short *)(const void *)(p + off);
        off += mp->index_count * 2;

        /* Uretici her parca sonunu 4 bayta hizaliyor (PPU hizasiz float
         * okuyamaz). Bu dolgu burada da atlanmazsa ikinci parcadan itibaren
         * offset kayar ve dosya bozuk gorunur. */
        off = (off + 3u) & ~3u;
    }

    /* sinir kutusu: kamera mesafesi ve olcek denetimi icin */
    for (i = 0; i < m->part_count; i++) {
        const MeshPart *mp = &m->part[i];
        unsigned int v;

        for (v = 0; v < mp->vertex_count; v++) {
            for (k = 0; k < 3; k++) {
                float c = mp->verts[v * 9 + k];

                if (c < m->min[k]) m->min[k] = c;
                if (c > m->max[k]) m->max[k] = c;
            }
        }
    }

    return 0;
}

int mesh_find(const Mesh *m, const char *name)
{
    int i;

    for (i = 0; i < m->part_count; i++)
        if (strcmp(m->part[i].name, name) == 0)
            return i;
    return -1;
}

int mesh_find_prefix(const Mesh *m, const char *prefix)
{
    size_t n = strlen(prefix);
    int i;

    for (i = 0; i < m->part_count; i++)
        if (strncmp(m->part[i].name, prefix, n) == 0)
            return i;
    return -1;
}

unsigned int mesh_triangles(const Mesh *m)
{
    unsigned int t = 0;
    int i;

    for (i = 0; i < m->part_count; i++)
        t += m->part[i].index_count / 3;
    return t;
}
