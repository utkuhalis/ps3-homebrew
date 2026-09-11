#ifndef MESH_H
#define MESH_H

/* End-to-end triangle mesh: the "BWM2" format produced by
 * tools/glb_to_mesh.py.
 *
 * The data comes embedded in the EBOOT and holds big-endian floats/u16, so
 * the PPU needs no byte swapping. The module only computes pointers; it
 * does not copy. */

/* In real aircraft models every moving surface is its own part: our own
 * generated jet has 27, the 737 asset 41. The limit was 16, then 40, and
 * both times the model was silently rejected - the mesh test caught both. */
#define MESH_MAX_PARTS 64

/* Vertex layout: x,y,z, nx,ny,nz, r,g,b, metallic */
#define MESH_VERTEX_FLOATS 10
#define MESH_VERTEX_BYTES  (MESH_VERTEX_FLOATS * 4)

typedef struct {
    char         name[32];
    unsigned int vertex_count;
    unsigned int index_count;
    const float *verts;         /* MESH_VERTEX_FLOATS floats */
    const unsigned short *idx;
} MeshPart;

typedef struct {
    int      part_count;
    MeshPart part[MESH_MAX_PARTS];
    /* bounding box of all parts (local space) */
    float    min[3];
    float    max[3];
} Mesh;

/* Parses the embedded data. 0 on success, -1 on a format error. */
int mesh_load(Mesh *m, const void *data, unsigned int size);

/* Find a part by name; -1 if not found */
int mesh_find(const Mesh *m, const char *name);

/* Find by prefix: in real models one surface may be split into several
 * panels (flap_left01..04). */
int mesh_find_prefix(const Mesh *m, const char *prefix);

/* Total triangle count (diagnostics) */
unsigned int mesh_triangles(const Mesh *m);

#endif
