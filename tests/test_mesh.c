/* Checks that the mesh parser matches the generator, on the REAL data file.
 *
 * Once, 4-byte alignment padding was added on the generator side but not
 * in the reader; the file looked corrupt and the game did not start. This
 * test catches that class of bug before it reaches the PS3. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../source/mesh.h"

/* Mesh data holds big-endian floats; the PS3's PPU reads them directly,
 * but the host (x86/arm) is little-endian, so reading through the same
 * pointer gives garbage. The test swaps the byte order when reading. */
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
        printf("FAIL: could not open data/plane.bin\n");
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buf = (unsigned char *)malloc((size_t)size);
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        printf("FAIL: could not read the file\n");
        return 1;
    }
    fclose(fp);

    if (mesh_load(&m, buf, (unsigned int)size) != 0) {
        printf("FAIL: mesh_load failed (generator/reader mismatch)\n");
        return 1;
    }

    /* Metallic must be in range: the shader scales the reflection by it, and a
     * broken value makes the whole body a mirror or matte. */
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
            printf("FAIL: metallic out of range in %d vertices\n", bad);
            failures++;
        } else {
            printf("test: metallic values valid\n");
        }
    }
    printf("test: mesh_load parses the real data (%d parts)\n",
           m.part_count);

    if (m.part_count < 6) {
        printf("FAIL: fewer parts than expected: %d\n", m.part_count);
        failures++;
    }

    /* If parsing is right, the end of the last part must land on the end of
     * the file: an offset drift shows up here. */
    {
        const unsigned char *last_end =
            (const unsigned char *)m.part[m.part_count - 1].idx
            + m.part[m.part_count - 1].index_count * 2;
        long used = last_end - buf;

        if (used > size || size - used > 4) {
            printf("FAIL: parsing does not land on the end of the file "
                   "(used %ld, file %ld)\n", used, size);
            failures++;
        } else {
            printf("test: part offsets land on the end of the file\n");
        }
    }

    /* Control surfaces must arrive as separate parts - the animation depends on it */
    {
        /* Parts EVERY model must have. The elevator and the wheel hubs vary by
         * model: in the 737 asset the horizontal stabiliser is part of the body
         * and the wheels sit inside the gear group, while our generated jet has
         * them as separate parts. So only surfaces every aircraft must have are
         * checked here. */
        const char *need[] = { "body", "flap_left", "flap_right",
                               "aileron_left", "aileron_right", "rudder",
                               "spoiler_left", "spoiler_right" };
        size_t n;

        for (n = 0; n < sizeof(need) / sizeof(need[0]); n++) {
            /* Parts may be numbered (flap_left01); the prefix is enough. */
            if (mesh_find_prefix(&m, need[n]) < 0) {
                printf("FAIL: missing part: %s\n", need[n]);
                failures++;
            }
        }
        if (failures == 0)
            printf("test: control surfaces exist as separate parts\n");
    }

    /* No part may exceed the u16 index limit */
    for (i = 0; i < m.part_count; i++) {
        if (m.part[i].vertex_count > 65535) {
            printf("FAIL: %s exceeds the u16 index limit\n", m.part[i].name);
            failures++;
        }
    }

    free(buf);

    if (failures == 0)
        printf("\n6 checks, 0 failures\n");
    else
        printf("\n%d HATA\n", failures);
    return failures ? 1 : 0;
}
