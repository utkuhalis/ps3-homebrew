#include <string.h>
#include <math.h>
#include <unistd.h>

#include <ppu-lv2.h>
#include <sys/thread.h>
#include <audio/audio.h>
#include <sys/event_queue.h>

#include "audio.h"

#define SAMPLE_RATE   48000.0f
#define CHANNELS      2
#define BLOCK_SAMPLES AUDIO_BLOCK_SAMPLES

static u32 port_num;
static audioPortConfig config;
static sys_event_queue_t snd_queue;
static sys_ipc_key_t snd_key;
static sys_ppu_thread_t snd_thread;

static volatile int running = 0;
static volatile int ready = 0;

/* Ana dongunun yazdigi, ses is parcaciginin okudugu hedef degerler.
 * Tek yazan-tek okuyan oldugu icin kilit kullanilmiyor; degerler yumusak
 * gectigi icin ara durumlar duyulmaz. */
static volatile float t_engine = 0.0f;   /* motor siddeti 0..1 */
static volatile float t_pitch  = 0.5f;   /* motor perdesi 0..1 */
static volatile float t_wind   = 0.0f;   /* ruzgar siddeti 0..1 */
static volatile float t_sea    = 0.0f;   /* deniz ugultusu 0..1 */
static volatile float t_warn   = 0.0f;   /* stall kornasi 0..1 */
static volatile float t_servo  = 0.0f;   /* takim/flap motoru 0..1 */
static volatile float t_roll   = 0.0f;   /* tekerlek yuvarlanmasi 0..1 */
static volatile float t_touch  = 0.0f;   /* tekerlek temasi (tek seferlik) */

/* Ses uretimi durumu */
static float ph_engine, ph_engine2, ph_engine3;
static float ph_warn, ph_servo, warn_gate;
static float lp_wind, lp_sea, lp_roll;
static float touch_env;
static unsigned int rng = 22222u;

static float frand(void)
{
    rng = rng * 1103515245u + 12345u;
    return (float)((rng >> 9) & 0xFFFF) / 32768.0f - 1.0f;
}

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Bir blok ses uretir: motor + ruzgar + deniz karisimi */
static void fill_block(float *out, float engine, float pitch,
                       float wind, float sea,
                       float warn, float servo, float roll)
{
    float base = 42.0f + pitch * 120.0f;    /* motor temel frekansi (Hz) */
    float step = base / SAMPLE_RATE * 6.28318531f;
    int i;

    for (i = 0; i < BLOCK_SAMPLES; i++) {
        float s, n;

        /* --- motor: temel + harmonikler + hafif gurultu --- */
        ph_engine  += step;
        ph_engine2 += step * 2.02f;
        ph_engine3 += step * 3.97f;

        if (ph_engine  > 6.28318531f) ph_engine  -= 6.28318531f;
        if (ph_engine2 > 6.28318531f) ph_engine2 -= 6.28318531f;
        if (ph_engine3 > 6.28318531f) ph_engine3 -= 6.28318531f;

        s = (sinf(ph_engine) * 0.55f
             + sinf(ph_engine2) * 0.28f
             + sinf(ph_engine3) * 0.14f) * engine;

        /* turbin ugultusu: yuksek frekansli gurultunun bir kismi */
        s += frand() * 0.05f * engine;

        /* --- ruzgar: alcak geciren suzgecten gecmis gurultu --- */
        n = frand();
        lp_wind += (n - lp_wind) * 0.06f;
        s += lp_wind * wind * 0.65f;

        /* --- deniz: daha kalin, daha yavas ugultu --- */
        n = frand();
        lp_sea += (n - lp_sea) * 0.012f;
        s += lp_sea * sea * 0.85f;

        /* --- stall kornasi: kesikli, tiz, dikkat cekici ---
         * Gercek ucaklardaki stall warning horn da sentetik bir tondur. */
        if (warn > 0.001f) {
            warn_gate += 3.6f / SAMPLE_RATE;     /* saniyede ~3.6 bip */
            if (warn_gate > 1.0f)
                warn_gate -= 1.0f;

            ph_warn += (820.0f / SAMPLE_RATE) * 6.28318531f;
            if (ph_warn > 6.28318531f)
                ph_warn -= 6.28318531f;

            if (warn_gate < 0.55f)
                s += sinf(ph_warn) * 0.30f * warn;
        }

        /* --- takim / flap motoru: mekanik vizilti + disli gurultusu --- */
        if (servo > 0.001f) {
            ph_servo += (95.0f / SAMPLE_RATE) * 6.28318531f;
            if (ph_servo > 6.28318531f)
                ph_servo -= 6.28318531f;

            s += (sinf(ph_servo) * 0.5f + frand() * 0.5f) * 0.18f * servo;
        }

        /* --- tekerlek yuvarlanmasi: pist dokusundan gelen gurultu --- */
        if (roll > 0.001f) {
            n = frand();
            lp_roll += (n - lp_roll) * 0.22f;
            s += lp_roll * roll * 0.40f;
        }

        /* --- tekerlek temasi: kisa, sonen ciyaklama --- */
        if (touch_env > 0.0005f) {
            n = frand();
            s += n * touch_env * 0.55f;
            touch_env *= 0.99965f;
        }

        s = clampf(s * 0.42f, -1.0f, 1.0f);

        out[i * CHANNELS + 0] = s;
        out[i * CHANNELS + 1] = s;
    }
}

static void sound_thread(void *arg)
{
    sys_event_t event;
    float engine = 0.0f, pitch = 0.5f, wind = 0.0f, sea = 0.0f;
    float warn = 0.0f, servo = 0.0f, roll = 0.0f;

    (void)arg;

    while (running) {
        float *buf;
        u64 current;

        /* ses donaniminin blok istemesini bekle */
        if (sysEventQueueReceive(snd_queue, &event, 20 * 1000) != 0)
            continue;

        /* hedeflere yumusak gecis: ani sicramalar tik sesi yapar */
        engine += (t_engine - engine) * 0.08f;
        pitch  += (t_pitch  - pitch)  * 0.06f;
        wind   += (t_wind   - wind)   * 0.05f;
        sea    += (t_sea    - sea)    * 0.04f;
        warn   += (t_warn   - warn)   * 0.25f;   /* uyari hizli girmeli */
        servo  += (t_servo  - servo)  * 0.30f;
        roll   += (t_roll   - roll)   * 0.15f;

        /* tek seferlik temas darbesi: tuketilince sifirlanir */
        if (t_touch > 0.0f) {
            touch_env = t_touch;
            t_touch = 0.0f;
        }

        /* readIndex, o an okunan blok numarasini tutan adrestir; siradaki
         * blok bir sonrakidir. */
        current = (*(u64 *)((u64)config.readIndex) + 1) % config.numBlocks;
        buf = (float *)((u64)config.audioDataStart
                        + (current * CHANNELS * BLOCK_SAMPLES * sizeof(float)));

        fill_block(buf, engine, pitch, wind, sea, warn, servo, roll);
    }

    sysThreadExit(0);
}

int audio_init(void)
{
    audioPortParam param;
    s32 rc;

    rc = audioInit();
    if (rc != 0)
        return -1;

    memset(&param, 0, sizeof(param));
    param.numChannels = AUDIO_PORT_2CH;
    param.numBlocks = AUDIO_BLOCK_8;
    param.attrib = 0;
    param.level = 1.0f;

    rc = audioPortOpen(&param, &port_num);
    if (rc != 0) {
        audioQuit();
        return -2;
    }

    audioGetPortConfig(port_num, &config);
    audioCreateNotifyEventQueue(&snd_queue, &snd_key);
    audioSetNotifyEventQueue(snd_key);
    sysEventQueueDrain(snd_queue);
    audioPortStart(port_num);

    running = 1;
    rc = sysThreadCreate(&snd_thread, sound_thread, NULL, 1500, 8192,
                         THREAD_JOINABLE, "ses");
    if (rc != 0) {
        running = 0;
        audioPortStop(port_num);
        audioPortClose(port_num);
        audioQuit();
        return -3;
    }

    ready = 1;
    return 0;
}

void audio_exit(void)
{
    u64 retval;

    if (!ready)
        return;

    running = 0;
    sysThreadJoin(snd_thread, &retval);

    audioPortStop(port_num);
    audioRemoveNotifyEventQueue(snd_key);
    audioPortClose(port_num);
    sysEventQueueDestroy(snd_queue, 0);
    audioQuit();
    ready = 0;
}

void audio_update(const Flight *f, const Atmosphere *atm)
{
    float speed_n, alt;

    if (!ready)
        return;

    /* motor: gaz kolu siddeti ve perdeyi belirler; yakit bitince susar */
    t_engine = (f->fuel_kg > 0.0f) ? (0.14f + f->throttle * 0.86f) : 0.0f;
    t_pitch  = clampf(f->throttle * 0.75f + f->airspeed / 260.0f, 0.0f, 1.0f);

    /* ruzgar: hava hizina gore; firtinada zaten daha gurultulu */
    speed_n = clampf(f->airspeed / 150.0f, 0.0f, 1.0f);
    t_wind = speed_n * (0.55f + atm->wave_scale * 0.22f);

    /* deniz: yaklastikca duyulur, yukseklerde kaybolur */
    alt = f->pos[1];
    t_sea = clampf(1.0f - (alt - WATER_SAFE_ALT) / 220.0f, 0.0f, 1.0f)
            * (0.35f + atm->wave_scale * 0.30f);

    /* --- stall kornasi: kritik acinin hemen altinda calmaya baslar --- */
    {
        float margin = STALL_ANGLE_RAD + f->flap * 0.06f - f->aoa;

        if (f->airspeed > 8.0f && margin < 0.05f)
            t_warn = clampf((0.05f - margin) / 0.05f, 0.0f, 1.0f);
        else
            t_warn = 0.0f;
    }

    /* --- takim/flap motoru: yuzey HAREKET EDERKEN duyulur --- */
    {
        static float last_gear = -1.0f;
        static float last_flap = -1.0f;
        float dg, df;

        if (last_gear < 0.0f) {
            last_gear = f->gear_pos;
            last_flap = f->flap;
        }

        dg = f->gear_pos - last_gear;
        df = f->flap - last_flap;
        if (dg < 0.0f) dg = -dg;
        if (df < 0.0f) df = -df;

        t_servo = clampf((dg + df) * 90.0f, 0.0f, 1.0f);

        last_gear = f->gear_pos;
        last_flap = f->flap;
    }

    /* --- tekerlekler: yerdeyken hizla orantili yuvarlanma sesi --- */
    {
        static int was_on_ground = 1;
        float horiz = sqrtf(f->vel[0] * f->vel[0] + f->vel[2] * f->vel[2]);

        if (f->on_ground && f->gear_pos > 0.5f)
            t_roll = clampf(horiz / 70.0f, 0.0f, 1.0f);
        else
            t_roll = 0.0f;

        /* havadan yere gecis: temas darbesi. Sertligi dikey hiza bagli. */
        if (f->on_ground && !was_on_ground) {
            float vsink = -f->vel[1];

            if (vsink < 0.0f)
                vsink = 0.0f;
            t_touch = clampf(0.25f + vsink * 0.12f, 0.0f, 1.0f);
        }
        was_on_ground = f->on_ground;
    }
}
