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
    Autopilot ap;

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
    ps3log("aircraft_init -> %d (%u ucgen)", rc,
           aircraft_triangle_count());
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

    rc = audio_init();
    ps3log("audio_init -> %d", rc);
    /* Ses acilamazsa oyun sessiz devam eder; oyunu durdurmaya degmez. */

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
    autopilot_init(&ap);
    atmosphere_compute(&atm, menu.weather, menu.time);
    objectives_init(&objs);
    /* Pistte, motor rolantide basla: kalkisi oyuncu yapar. */
    flight_init_on_runway(&plane, RUNWAY_POS[0], 0.55f,
                          RUNWAY_LENGTH * 0.42f);
    proj = mat4_perspective(FOV_DEG * 3.14159265f / 180.0f,
                            rsx3d_aspect(), Z_NEAR, Z_FAR);

    while (!should_exit) {
        float forward, strafe, updown, yaw_in, pitch_in;

        sysUtilCheckCallback();
        input_update();

        if (input_pressed(0, PAD_SELECT))
            gamemenu_toggle(&menu);

        /* Yuz tuslarinin bakis yedegi yalnizca serbest kamerada aciktir;
         * ucus modunda ayni tuslar flap/spoiler/takim/kamera icin kullanilir. */
        input_set_look_keys(!menu.open && cam_mode == CAM_FREE);

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

        /* --- ucus kumandalari (menu kapaliyken) --- */
        if (!menu.open) {
            float stick_p = -input_axis_left_y();
            float stick_r =  input_axis_left_x();

            /* R3: otopilot ac/kapa. Kumandaya dokunmak da otopilotu birakir -
             * gercek ucakta oldugu gibi pilot her an devralabilir. */
            if (input_pressed(0, PAD_R3))
                autopilot_toggle(&ap, &plane);

            if (ap.engaged &&
                (stick_p > 0.25f || stick_p < -0.25f ||
                 stick_r > 0.25f || stick_r < -0.25f))
                autopilot_disengage(&ap);

            plane.in_pitch = stick_p;
            plane.in_roll  = stick_r;
            plane.in_yaw   = 0.0f;

            /* Gaz: hem tetikler hem omuz tuslari. Emulatorun klavye
             * eslemesinde L2/R2 her zaman rahat erisilebilir olmadigi icin
             * L1/R1 de ayni isi yapar. */
            if (input_held(0, PAD_R2) || input_held(0, PAD_R1))
                plane.throttle += 0.6f * DT;
            if (input_held(0, PAD_L2) || input_held(0, PAD_L1))
                plane.throttle -= 0.6f * DT;
            if (plane.throttle < 0.0f) plane.throttle = 0.0f;
            if (plane.throttle > 1.0f) plane.throttle = 1.0f;

            /* KARE: flap kademesi (0 -> 1/3 -> 2/3 -> tam -> 0) */
            if (input_pressed(0, PAD_SQUARE)) {
                plane.flap += 0.34f;
                if (plane.flap > 1.01f)
                    plane.flap = 0.0f;
            }

            /* UCGEN: basili tutuldugu surece spoiler ve fren */
            if (input_held(0, PAD_TRIANGLE)) {
                plane.spoiler = 1.0f;
                plane.brakes = 1;
            } else {
                plane.spoiler = 0.0f;
                plane.brakes = 0;
            }

            /* CARPI: inis takimi ac/kapa */
            if (input_pressed(0, PAD_CROSS))
                plane.gear_down = !plane.gear_down;

            /* YUVARLAK: kamera modu (takip / kokpit / serbest) */
            if (input_pressed(0, PAD_CIRCLE))
                cam_mode = (CamMode)((cam_mode + 1) % CAM_MODE_COUNT);
        } else {
            plane.in_pitch = plane.in_roll = plane.in_yaw = 0.0f;
        }

        /* menuden secilen hava/saat her karede sahneye yansitilir */
        atmosphere_compute(&atm, menu.weather, menu.time);

        /* Otopilot kumanda girdilerini oyuncunun yerine uretir; fizik
         * degismez, ucak ayni modele tabidir. */
        autopilot_update(&ap, &plane, DT);

        flight_update(&plane, DT);

        if (cam_mode == CAM_FREE)
            camera_update(&cam, forward, strafe, updown, yaw_in, pitch_in, DT);
        else
            flightcam_update(&cam, cam_mode, &plane, DT);

        hud_update(&hud, &plane, DT);
        audio_update(&plane, &atm);
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
            hud_draw_controls(&plane);
            hud_draw_throttle_lever(&plane, &ap);
            hud_draw_warnings(&plane, frames);
            hud_draw_help(&plane);
            font_draw_text(24, 470, 1, flightcam_name(cam_mode),
                           RGB(170, 200, 235));
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

    audio_exit();
    ps3log("cikis: %lu kare cizildi", frames);
    ps3log_close();

    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
    input_exit();
    rsx3d_exit();

    return 0;
}
