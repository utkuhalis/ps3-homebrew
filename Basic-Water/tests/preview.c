/* Kamera ve projeksiyon matematigini host'ta dogrular.
 * Deniz izgarasi CPU'da tel-kafes olarak projekte edilip PPM'e yazilir; boylece
 * "kamera dogru yere bakiyor mu, ufuk nerede" sorusu RPCS3'e gitmeden gorulur.
 * Calistirma: tests/preview.sh */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "../source/mat4.h"
#include "../source/camera.h"

#define W 640
#define H 360

#define GRID_SPACING  50.0f
#define GRID_LINES    80      /* merkezden her yone kac cizgi */

static unsigned char fb[H][W][3];

static void clear_sky(void)
{
    int x, y;
    for (y = 0; y < H; y++) {
        /* ustte koyu, ufka dogru acilan gokyuzu */
        float t = (float)y / H;
        unsigned char r = (unsigned char)(90 + 90 * t);
        unsigned char g = (unsigned char)(140 + 80 * t);
        unsigned char b = (unsigned char)(210 + 40 * t);
        for (x = 0; x < W; x++) {
            fb[y][x][0] = r;
            fb[y][x][1] = g;
            fb[y][x][2] = b;
        }
    }
}

static void put(int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
    if (x < 0 || x >= W || y < 0 || y >= H)
        return;
    fb[y][x][0] = r;
    fb[y][x][1] = g;
    fb[y][x][2] = b;
}

#define NEAR_W 0.01f    /* w bu degerin altindaysa nokta kamera duzleminde/arkasinda */

/* Kirpilmis clip-space ucundan ekran koordinati uretir. */
static void to_screen(const float c[4], float *sx, float *sy)
{
    *sx = (c[0] / c[3] * 0.5f + 0.5f) * W;
    *sy = (1.0f - (c[1] / c[3] * 0.5f + 0.5f)) * H;
}

/* Ekran dikdortgenine gore parametrik kirpma (Liang-Barsky). */
static int clip_test(float p, float q, float *t0, float *t1)
{
    float r;

    if (fabsf(p) < 1e-6f)
        return q >= 0.0f;
    r = q / p;
    if (p < 0.0f) {
        if (r > *t1) return 0;
        if (r > *t0) *t0 = r;
    } else {
        if (r < *t0) return 0;
        if (r < *t1) *t1 = r;
    }
    return 1;
}

static void draw_line(const Mat4 *vp, const float a[3], const float b[3])
{
    float ca[4], cb[4];
    float ia[4] = { a[0], a[1], a[2], 1.0f };
    float ib[4] = { b[0], b[1], b[2], 1.0f };
    float ax, ay, bx, by, dx, dy, t0 = 0.0f, t1 = 1.0f;
    int steps, i, k;

    mat4_transform(vp, ia, ca);
    mat4_transform(vp, ib, cb);

    /* 1) yakin duzlem kirpmasi: kameranin arkasinda kalan kismi at */
    if (ca[3] < NEAR_W && cb[3] < NEAR_W)
        return;                                  /* tamamen arkada */

    if (ca[3] < NEAR_W || cb[3] < NEAR_W) {
        float t = (NEAR_W - ca[3]) / (cb[3] - ca[3]);
        float mid[4];

        for (k = 0; k < 4; k++)
            mid[k] = ca[k] + (cb[k] - ca[k]) * t;

        if (ca[3] < NEAR_W)
            memcpy(ca, mid, sizeof(ca));
        else
            memcpy(cb, mid, sizeof(cb));
    }

    to_screen(ca, &ax, &ay);
    to_screen(cb, &bx, &by);

    /* 2) ekran dikdortgenine kirpma - boylece cok uzun cizgiler de dogru cizilir */
    dx = bx - ax;
    dy = by - ay;
    if (!clip_test(-dx, ax - 0.0f, &t0, &t1)) return;
    if (!clip_test( dx, (W - 1) - ax, &t0, &t1)) return;
    if (!clip_test(-dy, ay - 0.0f, &t0, &t1)) return;
    if (!clip_test( dy, (H - 1) - ay, &t0, &t1)) return;

    bx = ax + dx * t1;
    by = ay + dy * t1;
    ax = ax + dx * t0;
    ay = ay + dy * t0;

    steps = (int)(fabsf(bx - ax) + fabsf(by - ay));
    if (steps < 1)
        steps = 1;

    for (i = 0; i <= steps; i++) {
        float t = (float)i / steps;
        put((int)(ax + (bx - ax) * t), (int)(ay + (by - ay) * t), 40, 90, 130);
    }
}

static void render(const Camera *cam, const char *path)
{
    Mat4 proj = mat4_perspective(60.0f * 3.14159265f / 180.0f,
                                 (float)W / H, 1.0f, 6000.0f);
    Mat4 view = camera_view_matrix(cam);
    Mat4 vp = mat4_mul(&proj, &view);
    float cx = floorf(cam->pos[0] / GRID_SPACING) * GRID_SPACING;
    float cz = floorf(cam->pos[2] / GRID_SPACING) * GRID_SPACING;
    float lo = -GRID_LINES * GRID_SPACING;
    float hi =  GRID_LINES * GRID_SPACING;
    FILE *f;
    int i, y, x;

    clear_sky();

    for (i = -GRID_LINES; i <= GRID_LINES; i++) {
        float off = i * GRID_SPACING;
        float a[3], b[3];

        /* z boyunca uzanan cizgi */
        a[0] = cx + off; a[1] = WATER_LEVEL; a[2] = cz + lo;
        b[0] = cx + off; b[1] = WATER_LEVEL; b[2] = cz + hi;
        draw_line(&vp, a, b);

        /* x boyunca uzanan cizgi */
        a[0] = cx + lo; a[1] = WATER_LEVEL; a[2] = cz + off;
        b[0] = cx + hi; b[1] = WATER_LEVEL; b[2] = cz + off;
        draw_line(&vp, a, b);
    }

    f = fopen(path, "wb");
    if (f == NULL) {
        fprintf(stderr, "yazilamadi: %s\n", path);
        exit(1);
    }
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++)
            fwrite(fb[y][x], 1, 3, f);
    fclose(f);
    printf("  yazildi: %s\n", path);
}

int main(void)
{
    Camera c;

    /* 1) baslangic: 20 birim yukseklikte, ufka bakiyor */
    camera_init(&c);
    render(&c, "build-test/01-baslangic.ppm");

    /* 2) asagi bakis */
    camera_init(&c);
    c.pos[1] = 120.0f;
    c.pitch = -0.6f;
    render(&c, "build-test/02-yukseklikten-asagi.ppm");

    /* 3) saga donmus */
    camera_init(&c);
    c.pos[1] = 60.0f;
    c.yaw = 0.7f;
    c.pitch = -0.25f;
    render(&c, "build-test/03-donmus.ppm");

    /* 4) suya en yakin konum (collider siniri) */
    camera_init(&c);
    {
        int i;
        for (i = 0; i < 600; i++)
            camera_update(&c, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f / 60.0f);
    }
    printf("  collider sonrasi yukseklik: %.3f (beklenen %.3f)\n",
           c.pos[1], WATER_LEVEL + MIN_ALTITUDE);
    render(&c, "build-test/04-su-seviyesi.ppm");

    return 0;
}
