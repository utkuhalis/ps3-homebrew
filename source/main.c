#include <stdio.h>
#include <math.h>

#include <ppu-lv2.h>
#include <sys/process.h>
#include <sysutil/sysutil.h>

#include "rsx3d.h"
#include "scene.h"
#include "camera.h"
#include "mat4.h"
#include "input.h"
#include "overlay.h"
#include "hud.h"
#include "font.h"
#include "gamemenu.h"
#include "atmosphere.h"
#include "weatherfx.h"
#include "gauges.h"
#include "objectives.h"
#include "minimap.h"
#include "runway.h"
#include "waypoint.h"
#include "flight.h"
#include "aircraft.h"
#include "flightcam.h"
#include "ps3log.h"
#include "audio.h"
#include "autopilot.h"
#include "profiler.h"
#include "texture.h"
#include "world.h"

SYS_PROCESS_PARAM(1001, 0x100000)

/* A narrow field of view (telephoto) enlarges objects and compresses
 * distances; that is how the scale of the aircraft and the sea comes
 * across. 60 degrees pushed everything away and made it look like a toy. */
#define FOV_DEG   46.0f
#define Z_NEAR     1.0f
#define Z_FAR   6000.0f
#define DT        (1.0f / 60.0f)

static int should_exit = 0;

static void sys_callback(u64 status, u64 param, void *userdata)
{
    (void)param;
    (void)userdata;
    if (status == SYSUTIL_EXIT_GAME)
        should_exit = 1;
}

int main(int argc, const char *argv[])
{
    Camera cam;
    Mat4 proj;
    int rc;
    unsigned long frames = 0;
    float time_sec;
    Hud hud;
    GameMenu menu;
    Atmosphere atm;
    Objectives objs;
    Flight plane;
    CamMode cam_mode = CAM_CHASE;
    Autopilot ap;

    (void)argc;
    (void)argv;

    ps3log_open();
    ps3log("basicwater starting");

    rc = rsx3d_init();
    ps3log("rsx3d_init -> %d (screen %dx%d)", rc,
           (int)rsx3d_width(), (int)rsx3d_height());
    if (rc < 0) {
        ps3log("STOPPED: rsx3d_init failed");
        ps3log_close();
        return 1;
    }


    rc = aircraft_init();
    ps3log("aircraft_init -> %d (%u triangles)", rc,
           aircraft_triangle_count());
    if (rc < 0) {
        printf("ERROR: aircraft_init failed (%d)\n", rc);
        rsx3d_exit();
        return 1;
    }

    rc = texture_init();
    ps3log("texture_init -> %d (%d textures)", rc, texture_count());
    /* If textures fail to load the game carries on without them; not worth stopping for. */

    rc = world_init_all();
    ps3log("world_init_all -> %d (%d worlds, selected: %s)", rc, world_count(),
           world_current()->name);
    if (rc < 0) {
        printf("ERROR: world_init_all failed (%d)\n", rc);
        rsx3d_exit();
        return 1;
    }

    rc = overlay_init();
    ps3log("overlay_init -> %d", rc);
    if (rc < 0) {
        printf("ERROR: overlay_init failed (%d)\n", rc);
        rsx3d_exit();
        return 1;
    }

    rc = audio_init();
    ps3log("audio_init -> %d", rc);
    /* If audio cannot be opened the game carries on silently; not worth stopping for. */

    ps3log("all init done, entering the main loop");

    if (input_init() != 0) {
        printf("ERROR: input_init failed\n");
        rsx3d_exit();
        return 1;
    }

    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sys_callback, NULL);

    camera_init(&cam);
    hud_init(&hud);
    gamemenu_init(&menu);
    autopilot_init(&ap);
    atmosphere_compute(&atm, menu.weather, menu.time);
    objectives_init(&objs);
    /* Start on the runway with the engine at idle: the player makes the
     * takeoff. The starting point comes from the selected world. */
    {
        float start_xz[2], heading, offset;

        world_current()->start_point(start_xz, &heading, &offset);
        flight_init_on_runway(&plane, start_xz, heading, offset);
    }
    proj = mat4_perspective(FOV_DEG * 3.14159265f / 180.0f,
                            rsx3d_aspect(), Z_NEAR, Z_FAR);

    while (!should_exit) {
        float forward, strafe, updown, yaw_in, pitch_in;

        prof_frame_begin();
        sysUtilCheckCallback();
        input_update();

        if (input_pressed(0, PAD_SELECT))
            gamemenu_toggle(&menu);

        /* The face-button look fallback is only on in the free camera; in the
         * flight modes the same buttons are flaps/spoiler/gear/camera. */
        input_set_look_keys(!menu.open && cam_mode == CAM_FREE);

        if (menu.open) {
            /* With the menu open the camera is frozen; input goes to the menu. */
            if (input_pressed(0, PAD_UP))    gamemenu_move(&menu, -1);
            if (input_pressed(0, PAD_DOWN))  gamemenu_move(&menu,  1);
            if (input_pressed(0, PAD_LEFT))  gamemenu_adjust(&menu, -1);
            if (input_pressed(0, PAD_RIGHT)) gamemenu_adjust(&menu,  1);
            if (input_pressed(0, PAD_CROSS)) gamemenu_confirm(&menu);

            if (menu.quit_requested)
                should_exit = 1;
        }

        if (input_pressed(0, PAD_START))
            should_exit = 1;

        /* Left stick: forward/back + strafe. The Y axis is positive down, so
         * forward = negative Y. */
        forward = -input_axis_left_y();
        strafe  =  input_axis_left_x();

        /* Right stick: look. The sign is flipped again so up looks up. */
        yaw_in   =  input_axis_right_x();
        pitch_in = -input_axis_right_y();

        updown = 0.0f;
        if (input_held(0, PAD_R1))
            updown += 1.0f;
        if (input_held(0, PAD_L1))
            updown -= 1.0f;

        if (menu.open) {
            forward = strafe = updown = 0.0f;
            yaw_in = pitch_in = 0.0f;
        }

        /* --- flight controls (menu closed) --- */
        if (!menu.open) {
            float stick_p = input_flight_pitch();
            float stick_r = input_flight_roll();

            /* R3: autopilot on/off. Touching the controls also disengages it - as on
             * a real aircraft, the pilot can take over at any moment. */
            if (input_pressed(0, PAD_R3))
                autopilot_toggle(&ap, &plane);

            if (ap.engaged &&
                (stick_p > 0.25f || stick_p < -0.25f ||
                 stick_r > 0.25f || stick_r < -0.25f))
                autopilot_disengage(&ap);

            plane.in_pitch = stick_p;
            plane.in_roll  = stick_r;
            plane.in_yaw   = 0.0f;

            /* Throttle: both the triggers and the shoulder buttons. L2/R2 are not
             * always easy to reach in the emulator's keyboard mapping, so L1/R1 do
             * the same job. */
            if (input_held(0, PAD_R2) || input_held(0, PAD_R1))
                plane.throttle += 0.6f * DT;
            if (input_held(0, PAD_L2) || input_held(0, PAD_L1))
                plane.throttle -= 0.6f * DT;
            if (plane.throttle < 0.0f) plane.throttle = 0.0f;
            if (plane.throttle > 1.0f) plane.throttle = 1.0f;

            /* SQUARE: move the flap lever one detent (0 -> 1/3 -> 2/3 -> full -> 0).
             * The surfaces follow the target at motor speed. */
            if (input_pressed(0, PAD_SQUARE)) {
                plane.flap_target += 0.34f;
                if (plane.flap_target > 1.01f)
                    plane.flap_target = 0.0f;
            }

            /* TRIANGLE: speed brake lever. Instead of hold-to-open it steps through
             * detents and STAYS there: half (in flight) -> full (landing) -> retracted.
             * On a real aircraft the lever also stays where it is put. */
            if (input_pressed(0, PAD_TRIANGLE)) {
                if (plane.spoiler_target < 0.01f)
                    plane.spoiler_target = 0.5f;
                else if (plane.spoiler_target < 0.51f)
                    plane.spoiler_target = 1.0f;
                else
                    plane.spoiler_target = 0.0f;
            }

            /* On the ground, full speed brake also means wheel brakes. */
            plane.brakes = (plane.on_ground && plane.spoiler > 0.5f);

            /* CROSS: landing gear up/down */
            if (input_pressed(0, PAD_CROSS))
                plane.gear_down = !plane.gear_down;

            /* CIRCLE: camera mode (chase / cockpit / wings / tail / free) */
            if (input_pressed(0, PAD_CIRCLE)) {
                cam_mode = (CamMode)((cam_mode + 1) % CAM_MODE_COUNT);
                flightcam_orbit_reset();
            }

            /* Left stick: orbit around the aircraft in the chase mode.
             * In cockpit and free mode it keeps its own function. */
            if (cam_mode == CAM_CHASE) {
                float ox = input_cam_x();
                float oy = input_cam_y();
                /* Zoom is on L3: the d-pad is already the pitch fallback, and binding both
                 * to the same keys would clash. */
                float zoom = input_held(0, PAD_L3) ? -1.0f : 0.0f;

                if (input_held(0, PAD_L3) && input_held(0, PAD_TRIANGLE))
                    zoom = 1.0f;

                flightcam_orbit(ox, oy, zoom, DT);
            }
        } else {
            plane.in_pitch = plane.in_roll = plane.in_yaw = 0.0f;
        }

        /* the weather/time chosen in the menu is applied to the scene every frame */
        atmosphere_compute(&atm, menu.weather, menu.time);

        /* The autopilot produces control inputs in the player's place; the
         * physics does not change, the aircraft obeys the same model. */
        prof_begin(PROF_FLIGHT);
        autopilot_update(&ap, &plane, DT);

        flight_update(&plane, DT);

        if (cam_mode == CAM_FREE) {
            camera_update(&cam, forward, strafe, updown, yaw_in, pitch_in, DT);

            /* The free camera itself only knows about sea level; it goes through the
             * same obstacle check so the runway surface and the aircraft count too. */
            {
                float g_cam = flightcam_ground_at(&plane, cam.pos[0],
                                                  cam.pos[2]);
                float g_pl = flightcam_ground_at(&plane, plane.pos[0],
                                                 plane.pos[2]);

                flightcam_clamp(&cam, plane.pos, aircraft_bound_radius(),
                                (g_cam > g_pl) ? g_cam : g_pl);
            }
        } else {
            flightcam_update(&cam, cam_mode, &plane, DT);
        }

        hud_update(&hud, &plane, DT);
        audio_update(&plane, &atm);
        objectives_update(&objs, &cam, flight_speed_kmh(&plane));
        prof_end(PROF_FLIGHT);

        time_sec = (float)(frames++) / 60.0f;

        /* log how far the first frames get */
        if (frames == 1)
            ps3log("first frame starting");
        else if (frames == 2)
            ps3log("first frame done (drawing and flip work)");
        else if (frames == 120)
            ps3log("120 frames done, scene running");

        rsx3d_begin_frame(SKY_CLEAR_COLOR);

        prof_begin(PROF_SCENE);
        world_current()->draw(&cam, &proj, time_sec, &atm);
        prof_end(PROF_SCENE);

        /* our own aircraft is not drawn in the cockpit view */
        prof_begin(PROF_AIRCRAFT);
        if (cam_mode != CAM_COCKPIT)
            aircraft_draw(&plane, &cam, &proj, &atm);
        prof_end(PROF_AIRCRAFT);

        /* 2D overlay: primitives are collected and sent over the scene in a single draw call */
        prof_begin(PROF_OVERLAY);
        overlay_begin();
        weatherfx_draw(&atm, time_sec);
        if (menu.hud_visible) {
            hud_draw(&hud, &cam);
            hud_draw_controls(&plane);
            hud_draw_throttle_lever(&plane, &ap);
            hud_draw_warnings(&plane, frames);
            hud_draw_help(&plane);
            font_draw_text(24, 470, 1, flightcam_name(cam_mode),
                           RGB(170, 200, 235));
            gauges_draw(flight_speed_kmh(&plane), flight_altitude(&plane),
                        plane.pitch, plane.roll);
            objectives_draw(&objs);
            minimap_draw(&plane);
            waypoint_draw_all(&cam, &proj);
        }
        gamemenu_draw(&menu);
        if (menu.show_profiler)
            hud_draw_profiler();
        prof_set_counts(aircraft_drawn_triangles(), overlay_rect_count());
        overlay_flush();
        prof_end(PROF_OVERLAY);

        prof_begin(PROF_FLIP);
        rsx3d_end_frame();
        prof_end(PROF_FLIP);
        prof_frame_end();

        /* Write the measurements to the log periodically too: the screen is hard
         * to read on a real PS3, the log can be fetched over FTP. */
        if (frames % 600 == 0 && frames > 0)
            ps3log("frame %.2f ms (%.0f fps) | flight %.2f model %.2f "
                   "scene %.2f runway %.2f plane %.2f hud %.2f flip %.2f "
                   "| tris %u rect %u",
                   prof_frame_us() / 1000.0f, prof_fps(),
                   prof_avg_us(PROF_FLIGHT) / 1000.0f,
                   prof_avg_us(PROF_MODEL) / 1000.0f,
                   prof_avg_us(PROF_SCENE) / 1000.0f,
                   prof_avg_us(PROF_RUNWAY) / 1000.0f,
                   prof_avg_us(PROF_AIRCRAFT) / 1000.0f,
                   prof_avg_us(PROF_OVERLAY) / 1000.0f,
                   prof_avg_us(PROF_FLIP) / 1000.0f,
                   prof_triangles(), prof_rects());
    }

    audio_exit();
    ps3log("exit: %lu frames drawn", frames);
    ps3log_close();

    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
    input_exit();
    rsx3d_exit();

    return 0;
}
