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
#include "gamemenu.h"
#include "atmosphere.h"
#include "weatherfx.h"
#include "gauges.h"
#include "objectives.h"
#include "minimap.h"
#include "runway.h"
#include "waypoint.h"

SYS_PROCESS_PARAM(1001, 0x100000)

#define FOV_DEG   60.0f
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

    (void)argc;
    (void)argv;

    rc = rsx3d_init();
    if (rc < 0) {
        printf("HATA: rsx3d_init basarisiz (%d)\n", rc);
        return 1;
    }

    rc = scene_init();
    if (rc < 0) {
        printf("HATA: scene_init basarisiz (%d)\n", rc);
        rsx3d_exit();
        return 1;
    }

    rc = runway_init();
    if (rc < 0) {
        printf("HATA: runway_init basarisiz (%d)\n", rc);
        rsx3d_exit();
        return 1;
    }

    rc = overlay_init();
    if (rc < 0) {
        printf("HATA: overlay_init basarisiz (%d)\n", rc);
        rsx3d_exit();
        return 1;
    }

    if (input_init() != 0) {
        printf("HATA: input_init basarisiz\n");
        rsx3d_exit();
        return 1;
    }

    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sys_callback, NULL);

    camera_init(&cam);
    hud_init(&hud);
    gamemenu_init(&menu);
    atmosphere_compute(&atm, menu.weather, menu.time);
    objectives_init(&objs);
    proj = mat4_perspective(FOV_DEG * 3.14159265f / 180.0f,
                            rsx3d_aspect(), Z_NEAR, Z_FAR);

    while (!should_exit) {
        float forward, strafe, updown, yaw_in, pitch_in;

        sysUtilCheckCallback();
        input_update();

        if (input_pressed(0, PAD_SELECT))
            gamemenu_toggle(&menu);

        input_set_look_keys(!menu.open);

        if (menu.open) {
            /* Menu acikken kamera dondurulur; girdi menuye gider. */
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

        /* Sol analog: ileri/geri + yana kayma. Y ekseni asagi pozitif oldugu
         * icin ileri = negatif Y. */
        forward = -input_axis_left_y();
        strafe  =  input_axis_left_x();

        /* Sag analog: bakis. Yukari bakmak icin yine isaret cevrilir. */
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

        /* menuden secilen hava/saat her karede sahneye yansitilir */
        atmosphere_compute(&atm, menu.weather, menu.time);
    objectives_init(&objs);

        camera_update(&cam, forward, strafe, updown, yaw_in, pitch_in, DT);
        hud_update(&hud, &cam, DT);
        objectives_update(&objs, &cam, hud.speed_kmh);

        time_sec = (float)(frames++) / 60.0f;

        rsx3d_begin_frame(SKY_CLEAR_COLOR);
        scene_draw(&cam, &proj, time_sec, &atm);
        runway_draw(&cam, &proj, &atm);

        /* 2D bindirme: dikdortgenler toplanir, sahnenin ustune tek
         * cizim cagrisiyla gonderilir */
        overlay_begin();
        weatherfx_draw(&atm, time_sec);
        if (menu.hud_visible) {
            hud_draw(&hud, &cam);
            gauges_draw(hud.speed_kmh, cam.pos[1], cam.pitch, 0.0f);
            objectives_draw(&objs);
            minimap_draw(&cam);
            waypoint_draw_all(&cam, &proj);
        }
        gamemenu_draw(&menu);
        overlay_flush();

        rsx3d_end_frame();
    }

    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
    input_exit();
    rsx3d_exit();

    return 0;
}
