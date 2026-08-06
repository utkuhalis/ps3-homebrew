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
#include "flight.h"
#include "aircraft.h"
#include "flightcam.h"
#include "ps3log.h"

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
    Flight plane;
    CamMode cam_mode = CAM_CHASE;

    (void)argc;
    (void)argv;

    ps3log_open();
    ps3log("basicwater basliyor");

    rc = rsx3d_init();
    ps3log("rsx3d_init -> %d (ekran %dx%d)", rc,
           (int)rsx3d_width(), (int)rsx3d_height());
    if (rc < 0) {
        ps3log("DURDU: rsx3d_init basarisiz");
        ps3log_close();
        return 1;
    }

    rc = scene_init();
    ps3log("scene_init -> %d", rc);
    if (rc < 0) {
        printf("HATA: scene_init basarisiz (%d)\n", rc);
        rsx3d_exit();
        return 1;
    }

    rc = aircraft_init();
    ps3log("aircraft_init -> %d", rc);
    if (rc < 0) {
        printf("HATA: aircraft_init basarisiz (%d)\n", rc);
        rsx3d_exit();
        return 1;
    }

    rc = runway_init();
    ps3log("runway_init -> %d", rc);
    if (rc < 0) {
        printf("HATA: runway_init basarisiz (%d)\n", rc);
        rsx3d_exit();
        return 1;
    }

    rc = overlay_init();
    ps3log("overlay_init -> %d", rc);
    if (rc < 0) {
        printf("HATA: overlay_init basarisiz (%d)\n", rc);
        rsx3d_exit();
        return 1;
    }

    ps3log("tum baslatmalar tamam, ana donguye giriliyor");

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
    {
        /* kalkis pistinin biraz gerisinde ve uzerinde havada basla */
        float start[3];

        start[0] = RUNWAY_POS[0][0];
        start[1] = 120.0f;
        start[2] = RUNWAY_POS[0][1] + 700.0f;
        flight_init(&plane, start, 3.14159265f);
    }
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
    {
        /* kalkis pistinin biraz gerisinde ve uzerinde havada basla */
        float start[3];

        start[0] = RUNWAY_POS[0][0];
        start[1] = 120.0f;
        start[2] = RUNWAY_POS[0][1] + 700.0f;
        flight_init(&plane, start, 3.14159265f);
    }

        flight_update(&plane, DT);

        if (cam_mode == CAM_FREE)
            camera_update(&cam, forward, strafe, updown, yaw_in, pitch_in, DT);
        else
            flightcam_update(&cam, cam_mode, &plane, DT);

        hud_update(&hud, &plane, DT);
        objectives_update(&objs, &cam, flight_speed_kmh(&plane));

        time_sec = (float)(frames++) / 60.0f;

        /* ilk karelerde nereye kadar gidildigi kayda gecer */
        if (frames == 1)
            ps3log("ilk kare basliyor");
        else if (frames == 2)
            ps3log("ilk kare tamamlandi (cizim ve flip calisiyor)");
        else if (frames == 120)
            ps3log("120 kare tamamlandi, sahne akiyor");

        rsx3d_begin_frame(SKY_CLEAR_COLOR);
        scene_draw(&cam, &proj, time_sec, &atm);
        runway_draw(&cam, &proj, &atm);

        /* kokpit gorusunde kendi ucagimizi cizmiyoruz */
        if (cam_mode != CAM_COCKPIT)
            aircraft_draw(&plane, &cam, &proj, &atm);

        /* 2D bindirme: dikdortgenler toplanir, sahnenin ustune tek
         * cizim cagrisiyla gonderilir */
        overlay_begin();
        weatherfx_draw(&atm, time_sec);
        if (menu.hud_visible) {
            hud_draw(&hud, &cam);
            gauges_draw(flight_speed_kmh(&plane), flight_altitude(&plane),
                        plane.pitch, plane.roll);
            objectives_draw(&objs);
            minimap_draw(&cam);
            waypoint_draw_all(&cam, &proj);
        }
        gamemenu_draw(&menu);
        overlay_flush();

        rsx3d_end_frame();
    }

    ps3log("cikis: %lu kare cizildi", frames);
    ps3log_close();

    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
    input_exit();
    rsx3d_exit();

    return 0;
}
