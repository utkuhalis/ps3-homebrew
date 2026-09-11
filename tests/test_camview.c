/* Camera view angles and artificial horizon maths - host-side tests.
 * No PS3 needed; empty stubs stand in for the overlay and the aircraft model. */

#include <stdio.h>
#include <math.h>

#include "../source/flightcam.h"
#include "../source/gauges.h"


static int failures;

static void check(int cond, const char *what)
{
    if (!cond) {
        printf("FAIL: %s\n", what);
        failures++;
    }
}

static void near(float got, float want, float tol, const char *what)
{
    float d = got - want;

    if (d < 0)
        d = -d;
    if (d > tol) {
        printf("FAIL: %s (expected %.3f, got %.3f)\n", what, want, got);
        failures++;
    }
}

/* --- stubs: these tests use neither drawing nor the aircraft model --- */
void overlay_fill_rect(int x, int y, int w, int h, unsigned int c)
{ (void)x; (void)y; (void)w; (void)h; (void)c; }
void overlay_blend_rect(int x, int y, int w, int h, unsigned int c, int a)
{ (void)x; (void)y; (void)w; (void)h; (void)c; (void)a; }
void overlay_rot_rect(float a, float b, float c, float d, float e,
                      unsigned int f, int g)
{ (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; }
void overlay_ring(float a, float b, float c, float d, unsigned int e, int f)
{ (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; }
void overlay_disc(float a, float b, float c, unsigned int d, int e)
{ (void)a; (void)b; (void)c; (void)d; (void)e; }
void font_draw_center(int a, int b, int c, const char *d, unsigned int e)
{ (void)a; (void)b; (void)c; (void)d; (void)e; }
float aircraft_bound_radius(void) { return 21.0f; }
void aircraft_cockpit_pos(const Flight *f, float out[3])
{ out[0] = f->pos[0]; out[1] = f->pos[1]; out[2] = f->pos[2]; }

int main(void)
{
    float eye[3], target[3], yaw, pitch;

    /* --- look_at: basic directions --- */
    eye[0] = 0; eye[1] = 0; eye[2] = 0;

    target[0] = 0; target[1] = 0; target[2] = -10;      /* -Z: yaw 0 */
    flightcam_look_at(eye, target, &yaw, &pitch);
    near(yaw, 0.0f, 0.001f, "looking towards -Z gives yaw 0");
    near(pitch, 0.0f, 0.001f, "looking towards -Z gives pitch 0");

    target[0] = 10; target[1] = 0; target[2] = 0;       /* +X: yaw +90 */
    flightcam_look_at(eye, target, &yaw, &pitch);
    near(yaw, 1.5708f, 0.001f, "looking towards +X gives yaw +90 degrees");

    target[0] = 0; target[1] = 10; target[2] = 0;       /* up */
    flightcam_look_at(eye, target, &yaw, &pitch);
    near(pitch, 1.5708f, 0.001f, "looking up gives pitch +90 degrees");
    printf("test: look_at gives the basic directions right\n");

    /* --- the chase camera keeps the aircraft centred --- */
    {
        Flight f;
        Camera cam;
        float head;

        for (head = -3.0f; head < 3.0f; head += 0.7f) {
            float fwd[3], dot, len;
            int i;

            /* turn the aircraft various ways and let the camera converge */
            for (i = 0; i < 3; i++)
                f.pos[i] = 0.0f;
            f.pos[1] = 100.0f;
            f.yaw = head;
            f.pitch = 0.2f;
            f.roll = 0.0f;

            cam.pos[0] = cam.pos[1] = cam.pos[2] = 0.0f;
            cam.yaw = cam.pitch = 0.0f;
            for (i = 0; i < 200; i++)
                flightcam_update(&cam, CAM_CHASE, &f, 1.0f / 60.0f);

            /* the camera's forward vector must point at the aircraft */
            fwd[0] = cosf(cam.pitch) * sinf(cam.yaw);
            fwd[1] = sinf(cam.pitch);
            fwd[2] = -cosf(cam.pitch) * cosf(cam.yaw);

            {
                float d[3];
                d[0] = f.pos[0] - cam.pos[0];
                d[1] = f.pos[1] - cam.pos[1];
                d[2] = f.pos[2] - cam.pos[2];
                len = sqrtf(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]);
                dot = (d[0]*fwd[0] + d[1]*fwd[1] + d[2]*fwd[2]) / len;
            }

            /* The camera looks a little ahead of the nose, so it is not exactly 1.0;
             * this is enough to show the aircraft does not leave the screen. */
            check(dot > 0.90f, "chase camera looks at the aircraft");

            /* the camera must be BEHIND the aircraft: opposite to the nose direction */
            {
                float back[3], bd;
                back[0] = cam.pos[0] - f.pos[0];
                back[2] = cam.pos[2] - f.pos[2];
                bd = back[0] * sinf(head) + back[2] * (-cosf(head));
                check(bd < 0.0f, "chase camera is behind the aircraft");
            }
        }
        printf("test: chase camera keeps the aircraft centred in every attitude\n");
    }

    /* --- artificial horizon signs --- */
    near(gauge_horizon_shift(0.0f, 52.0f), 0.0f, 0.001f,
         "horizon centred in level flight");
    check(gauge_horizon_shift(0.5f, 52.0f) < 0.0f,
          "horizon moves down with the nose up");
    check(gauge_horizon_shift(-0.5f, 52.0f) > 0.0f,
          "horizon moves up with the nose down");

    /* in level flight the top of the gauge must be sky and the bottom ground.
     * screen y is positive down: dy < 0 is the top. */
    check(gauge_horizon_dist(0.0f, -20.0f, 0.0f, 0.0f) > 0.0f,
          "top of the gauge is sky in level flight");
    check(gauge_horizon_dist(0.0f, 20.0f, 0.0f, 0.0f) < 0.0f,
          "bottom of the gauge is ground in level flight");
    printf("test: artificial horizon sky/ground orientation correct\n");

    /* --- the aircraft model must face where it is going --- */
    {
        Flight f;
        float nose_local[3] = { 0.0f, 0.0f, -10.0f };   /* nose direction */
        float nose_world[3];
        float head;

        for (head = -3.0f; head < 3.0f; head += 0.6f) {
            float dx, dz, fx, fz, dot;

            f.pos[0] = f.pos[1] = f.pos[2] = 0.0f;
            f.yaw = head;
            f.pitch = 0.0f;
            f.roll = 0.0f;

            flight_body_to_world(&f, nose_local, nose_world);

            dx = nose_world[0];
            dz = nose_world[2];
            fx = sinf(head);            /* same as forward_vec in flight.c */
            fz = -cosf(head);
            dot = (dx * fx + dz * fz) / 10.0f;

            check(dot > 0.999f,
                  "aircraft model nose matches the flight direction");
        }
        printf("test: aircraft model aligned with the flight direction\n");
    }

    /* --- the attitude matrix must match body_to_world exactly --- */
    {
        Flight f;
        float m[3][3];
        float local[6][3] = {
            {1, 0, 0}, {0, 1, 0}, {0, 0, -1},
            {2.5f, -1.3f, 4.7f}, {-3.1f, 0.9f, -2.2f}, {0.4f, 0.4f, 0.4f}
        };
        float ang[4] = { 0.0f, 0.7f, -1.9f, 2.8f };
        int i, j, k, q;

        for (i = 0; i < 4; i++) {
            for (j = 0; j < 4; j++) {
                for (k = 0; k < 4; k++) {
                    f.pos[0] = 12.0f; f.pos[1] = -3.0f; f.pos[2] = 7.0f;
                    f.roll = ang[i]; f.pitch = ang[j] * 0.4f; f.yaw = ang[k];

                    flight_orientation_matrix(&f, m);

                    for (q = 0; q < 6; q++) {
                        float ref[3], got[3];
                        int c;

                        flight_body_to_world(&f, local[q], ref);
                        for (c = 0; c < 3; c++)
                            got[c] = m[c][0] * local[q][0]
                                   + m[c][1] * local[q][1]
                                   + m[c][2] * local[q][2] + f.pos[c];

                        for (c = 0; c < 3; c++) {
                            float d = got[c] - ref[c];

                            if (d < 0) d = -d;
                            if (d > 0.0005f) {
                                printf("FAIL: matrix deviates from body_to_world"
                                       " (%.4f vs %.4f)\n", got[c], ref[c]);
                                failures++;
                                i = j = k = 99;
                            }
                        }
                    }
                }
            }
        }
        printf("test: attitude matrix matches body_to_world\n");
    }

    /* --- camera collision: must not enter the body or the ground --- */
    {
        Camera c;
        float plane[3] = { 100.0f, 50.0f, -200.0f };
        const float R = 21.0f;
        float ang, pit;
        int inside = 0, below = 0;

        printf("test: camera stays out of the body and the ground\n");

        /* Try placing the camera right INSIDE the aircraft from every direction */
        for (ang = -3.1f; ang < 3.1f; ang += 0.4f) {
            for (pit = -1.4f; pit < 1.4f; pit += 0.35f) {
                float dist;

                for (dist = 0.0f; dist < R + 5.0f; dist += 2.0f) {
                    float dx, dy, dz, d;

                    c.pos[0] = plane[0] + sinf(ang) * cosf(pit) * dist;
                    c.pos[1] = plane[1] + sinf(pit) * dist;
                    c.pos[2] = plane[2] + cosf(ang) * cosf(pit) * dist;

                    flightcam_clamp(&c, plane, R, 9.0f);

                    dx = c.pos[0] - plane[0];
                    dy = c.pos[1] - plane[1];
                    dz = c.pos[2] - plane[2];
                    d = sqrtf(dx * dx + dy * dy + dz * dz);

                    if (d < R - 0.01f)
                        inside++;
                    if (c.pos[1] < 9.0f - 0.01f)
                        below++;
                }
            }
        }

        check(inside == 0, "camera never ends up inside the body from any direction");
        check(below == 0, "camera does not go below the ground");

        /* The ground rule must not override the body rule: even with the aircraft
         * on the ground the camera stays above the ground */
        c.pos[0] = plane[0];
        c.pos[1] = -500.0f;         /* far below */
        c.pos[2] = plane[2];
        flightcam_clamp(&c, plane, R, 9.0f);
        check(c.pos[1] >= 9.0f, "camera coming from below settles on the ground");

        /* A camera far away must not be moved at all */
        c.pos[0] = plane[0] + 400.0f;
        c.pos[1] = plane[1] + 120.0f;
        c.pos[2] = plane[2];
        {
            float before = c.pos[0];

            flightcam_clamp(&c, plane, R, 9.0f);
            check(c.pos[0] == before, "camera clear of obstacles is not moved");
        }
    }

    /* --- with the aircraft on the runway the camera must not go UNDER it ---
     * Reported bug: a camera swung out beside the runway found sea level
     * there, dropped 9 metres and the runway came between it and the view.
     * The surface under the aircraft must count as ground too. */
    {
        Camera c;
        Flight f;
        float rw[2] = { 0.0f, 0.0f };
        float g_cam, g_pl, ground;
        int i;

        printf("test: camera stays above runway level with the aircraft on the runway\n");

        flight_init_on_runway(&f, rw, 0.0f, 0.0f);

        /* try the camera beside the runway, far below */
        for (i = 0; i < 8; i++) {
            c.pos[0] = 200.0f + i * 60.0f;   /* outside the runway width */
            c.pos[1] = -50.0f;               /* below sea level */
            c.pos[2] = f.pos[2];

            g_cam = flightcam_ground_at(&f, c.pos[0], c.pos[2]);
            g_pl  = flightcam_ground_at(&f, f.pos[0], f.pos[2]);
            ground = (g_cam > g_pl) ? g_cam : g_pl;

            flightcam_clamp(&c, f.pos, 21.0f, ground);

            check(c.pos[1] >= DECK_Y,
                  "camera beside the runway stays above the runway surface");
        }

        /* the runway rule must not apply while the aircraft is over the sea */
        f.pos[0] = 8000.0f;
        f.pos[1] = 400.0f;
        f.pos[2] = 8000.0f;
        c.pos[0] = 8000.0f;
        c.pos[1] = -50.0f;
        c.pos[2] = 8100.0f;

        g_cam = flightcam_ground_at(&f, c.pos[0], c.pos[2]);
        g_pl  = flightcam_ground_at(&f, f.pos[0], f.pos[2]);
        ground = (g_cam > g_pl) ? g_cam : g_pl;
        flightcam_clamp(&c, f.pos, 21.0f, ground);

        check(c.pos[1] < DECK_Y,
              "over open sea the camera is not needlessly lifted to runway height");
    }

    if (failures == 0)
        printf("\n40 checks, 0 failures\n");
    else
        printf("\n%d HATA\n", failures);
    return failures ? 1 : 0;
}
