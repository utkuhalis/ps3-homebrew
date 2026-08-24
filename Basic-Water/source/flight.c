#include <math.h>
#include <string.h>

#include "flight.h"

#define AIR_DENSITY      1.225f
#define CL_SLOPE         5.2f    /* hucum acisi basina tasima katsayisi */
#define CL_FLAP          0.85f   /* tam flap'in ekledigi tasima */
#define CD_BASE          0.028f
#define CD_INDUCED       0.055f
#define CD_FLAP          0.10f
#define CD_SPOILER       0.16f
#define SPOILER_LIFT_LOSS 0.55f  /* tam spoiler tasimanin bu oranini kirar */

/* Tam girdide donme hizlari (radyan/saniye). Ilk degerler (1.25 / 2.10)
 * saniyede 71 ve 120 derece ediyordu; ucak ucaktan cok oyuncak gibi
 * donuyordu. Bir is jeti icin 30-50 derece/saniye gercekci. */
#define PITCH_RATE       0.34f
#define ROLL_RATE        0.85f
#define YAW_RATE         0.26f

/* Statik kararlilik: burun kendiliginden hava akisi yonune doner.
 * Gercek ucagi "kendi ucan" bir sey yapan sey budur. */
#define PITCH_STABILITY  0.85f
#define FUEL_BURN_KGS    2.20f   /* 737 tam gazda ~2.2 kg/s */

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static float len3(const float v[3])
{
    return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

/* Burun yonu (yaw/pitch'ten). Roll burun yonunu degistirmez. */
static void forward_vec(const Flight *f, float out[3])
{
    float cp = cosf(f->pitch);

    out[0] =  cp * sinf(f->yaw);
    out[1] =  sinf(f->pitch);
    out[2] = -cp * cosf(f->yaw);
}

/* Yukari yon: govde (0,1,0) ekseninin dunyadaki karsiligi.
 * Yonelim matrisinden turetilir; boylece tasima yonu ile modelin gorsel
 * yatisi hicbir zaman ayrisamaz. */
static void up_vec(const Flight *f, float out[3])
{
    float m[3][3];

    flight_orientation_matrix(f, m);
    out[0] = m[0][1];
    out[1] = m[1][1];
    out[2] = m[2][1];
}

void flight_orientation_matrix(const Flight *f, float m[3][3]);

/* Tasima katsayisi: hucum acisiyla dogrusal artar, stall acisindan sonra
 * hizla coker. Flap hem katsayiyi hem kritik aciyi artirir. */
static float lift_coefficient(float aoa, float flap)
{
    float stall = STALL_ANGLE_RAD + flap * 0.06f;
    float cl = aoa * CL_SLOPE + flap * CL_FLAP;
    float mag = aoa < 0.0f ? -aoa : aoa;
    float over;

    /* Kanat her iki yonde de stall eder. Onceden yalnizca pozitif taraf
     * sinirlaniyordu; burun asagi dalista hucum acisi -1.3 radyana kadar
     * gidip tasima katsayisi -6'ya ciktigi icin ucak yanlara firliyordu. */
    if (mag <= stall)
        return cl;

    over = (mag - stall) / 0.20f;
    if (over > 1.0f)
        over = 1.0f;
    cl *= (1.0f - 0.85f * over);

    /* Cok buyuk acilarda kanat artik kanat degildir: tasima sifira gider. */
    if (mag > 1.05f) {
        float fade = (1.57f - mag) / 0.52f;

        if (fade < 0.0f)
            fade = 0.0f;
        cl *= fade;
    }

    return cl;
}

float flight_lift_force(float airspeed, float aoa, float flap, float spoiler)
{
    float cl = lift_coefficient(aoa, flap);
    float q = 0.5f * AIR_DENSITY * airspeed * airspeed * WING_AREA_M2;

    cl *= (1.0f - SPOILER_LIFT_LOSS * spoiler);
    return cl * q;
}

float flight_drag_force(float airspeed, float aoa, float flap, float spoiler)
{
    return flight_drag_force_full(airspeed, aoa, flap, spoiler, 0);
}

float flight_drag_force_full(float airspeed, float aoa, float flap,
                             float spoiler, int gear_down)
{
    float cl = lift_coefficient(aoa, flap);
    float cd = CD_BASE + CD_INDUCED * cl * cl
               + CD_FLAP * flap + CD_SPOILER * spoiler
               + (gear_down ? GEAR_DRAG : 0.0f);
    float q = 0.5f * AIR_DENSITY * airspeed * airspeed * WING_AREA_M2;

    return cd * q;
}

/* Govde koordinatini dunyaya tasir: once roll (Z), sonra pitch (X),
 * sonra yaw (Y).
 *
 * Yaw, burun yonuyle ayni konvansiyonda olmali: yerel -Z ekseni dunyada
 * (sin yaw, 0, -cos yaw) yonune gider - forward_vec ile birebir ayni.
 * X bileseni ters yazildigi icin ucak modeli, ucagin gercekte gittigi
 * yonden farkli bir yone bakiyordu ve pistte yamuk duruyordu. */
void flight_body_to_world(const Flight *f, const float in[3], float out[3])
{
    float cr = cosf(f->roll),  sr = sinf(f->roll);
    float cp = cosf(f->pitch), sp = sinf(f->pitch);
    float cy = cosf(f->yaw),   sy = sinf(f->yaw);
    float x1, y1, z1, x2, y2, z2;

    /* Pozitif roll SAGA yatistir: sag kanat asagi iner. Ilk yazimda isaret
     * tersti; model sola yatarken fizik (yaw += sin(roll)) saga donduruyordu,
     * yani kumanda gorsel olarak ters calisiyordu. */
    x1 = in[0] * cr + in[1] * sr;
    y1 = -in[0] * sr + in[1] * cr;
    z1 = in[2];

    y2 = y1 * cp - z1 * sp;
    z2 = y1 * sp + z1 * cp;
    x2 = x1;

    out[0] = f->pos[0] + (x2 * cy - z2 * sy);
    out[1] = f->pos[1] + y2;
    out[2] = f->pos[2] + (x2 * sy + z2 * cy);
}

/* Yonelim matrisi (3x3, satir oncelikli): flight_body_to_world'un oteleme
 * disindaki kismi. Cok vertexli model cizerken trigonometriyi vertex basina
 * degil kare basina bir kez hesaplamak icin ayrildi.
 *
 * Katsayilar flight_body_to_world acilarak turetildi; test bu ikisinin
 * birebir ayni sonucu verdigini dogrular. */
void flight_orientation_matrix(const Flight *f, float m[3][3])
{
    float cr = cosf(f->roll),  sr = sinf(f->roll);
    float cp = cosf(f->pitch), sp = sinf(f->pitch);
    float cy = cosf(f->yaw),   sy = sinf(f->yaw);

    m[0][0] =  cr * cy + sr * sp * sy;
    m[0][1] =  sr * cy - cr * sp * sy;
    m[0][2] = -cp * sy;

    m[1][0] = -sr * cp;
    m[1][1] =  cr * cp;
    m[1][2] = -sp;

    m[2][0] =  cr * sy - sr * sp * cy;
    m[2][1] =  sr * sy + cr * sp * cy;
    m[2][2] =  cp * cy;
}

/* Ucak pist dikdortgeninin uzerinde mi?
 *
 * dx,dz pist merkezine gore dunya farki. Once pist yonune gore dondurulup
 * uzunluk/genislik eksenlerine ayristirilir. Saf fonksiyon: birim testli. */
int flight_over_runway(const Flight *f, float dx, float dz)
{
    float ch = cosf(f->ground_heading);
    float sh = sinf(f->ground_heading);

    /* pist ekseni boyunca (burun yonu) ve enine bilesenler */
    float along = dx * sh - dz * ch;
    float across = dx * ch + dz * sh;

    if (along < 0.0f) along = -along;
    if (across < 0.0f) across = -across;

    return (along <= f->ground_half_len && across <= f->ground_half_wid);
}

void flight_init_on_runway(Flight *f, const float runway_xz[2], float heading,
                           float start_offset)
{
    memset(f, 0, sizeof(*f));

    /* Pistin BASINDA, tekerlekler uzerinde, duruyor. Onceden pist merkezine
     * konuyordu ve kalkis kosusuna yalnizca yarim pist kaliyordu. */
    f->pos[0] = runway_xz[0] - sinf(heading) * start_offset;
    f->pos[1] = DECK_Y + GEAR_HEIGHT;
    f->pos[2] = runway_xz[1] + cosf(heading) * start_offset;

    f->yaw = heading;
    f->throttle = 0.0f;         /* motor rolantide, kalkisi oyuncu yapar */
    f->fuel_kg = FUEL_FULL_KG;
    f->mass_kg = EMPTY_MASS_KG + FUEL_FULL_KG;

    f->gear_down = 1;
    f->gear_pos = 1.0f;
    f->flap = 0.34f;            /* kalkis icin bir kademe flap */
    f->on_ground = 1;
    f->airborne = 0;

    f->ground_ref[0] = runway_xz[0];
    f->ground_ref[1] = DECK_Y;
    f->ground_ref[2] = runway_xz[1];
    f->ground_heading = heading;
    f->ground_half_len = RUNWAY_HALF_LEN;
    f->ground_half_wid = RUNWAY_HALF_WID;
}

void flight_init(Flight *f, const float start_pos[3], float start_yaw)
{
    memset(f, 0, sizeof(*f));

    f->pos[0] = start_pos[0];
    f->pos[1] = start_pos[1];
    f->pos[2] = start_pos[2];

    f->yaw = start_yaw;
    f->throttle = 0.35f;
    f->fuel_kg = FUEL_FULL_KG;
    f->mass_kg = EMPTY_MASS_KG + FUEL_FULL_KG;
    f->gear_down = 0;
    f->gear_pos = 0.0f;
    f->airborne = 1;

    /* havada baslar: burun yonunde bir miktar hiz */
    {
        float fwd[3];
        forward_vec(f, fwd);
        /* 737 seyir hizina yakin baslangic; 70 m/s bu govde icin stall alti */
        f->vel[0] = fwd[0] * 190.0f;
        f->vel[1] = fwd[1] * 190.0f;
        f->vel[2] = fwd[2] * 190.0f;
    }
}

void flight_update(Flight *f, float dt)
{
    float fwd[3], up[3];
    float speed, lift, drag, thrust;
    float acc[3];
    int i;

    if (dt <= 0.0f)
        return;

    /* --- kumanda girdileri yonelimi degistirir ---
     *
     * Girdi dogrudan aciyi degil, hedef ACISAL HIZI belirler; gercek acisal
     * hiz hedefe atalet zaman sabitiyle yaklasir. Boylece ucagin kutlesi
     * hissedilir: kumanda aninda karsilik vermez, birakildiginda donme
     * hemen kesilmez.
     *
     * Etkinlik ayrica dinamik basincla (hizin karesiyle) olceklenir: yerde
     * duran ucak kumandaya tepki vermez, yavas ucusta kumandalar agirdir. */
    {
        float v = len3(f->vel);
        float q = (v * v) / (CONTROL_REF_MS * CONTROL_REF_MS);
        float kp, kr, ky;

        if (q > 1.35f)
            q = 1.35f;

        kp = dt / PITCH_INERTIA_S;
        kr = dt / ROLL_INERTIA_S;
        ky = dt / YAW_INERTIA_S;
        if (kp > 1.0f) kp = 1.0f;
        if (kr > 1.0f) kr = 1.0f;
        if (ky > 1.0f) ky = 1.0f;

        f->p_rate += (f->in_pitch * PITCH_RATE * q - f->p_rate) * kp;
        f->r_rate += (f->in_roll  * ROLL_RATE  * q - f->r_rate) * kr;
        f->y_rate += (f->in_yaw   * YAW_RATE   * q - f->y_rate) * ky;

        f->pitch += f->p_rate * dt;
        f->roll  += f->r_rate * dt;
        f->yaw   += f->y_rate * dt;
    }

    /* yatis burnu cevirir (koordineli donus) */
    f->yaw += sinf(f->roll) * 0.55f * dt;

    f->pitch = clampf(f->pitch, -1.30f, 1.30f);

    while (f->roll >  3.14159265f) f->roll -= 6.28318531f;
    while (f->roll < -3.14159265f) f->roll += 6.28318531f;
    while (f->yaw  >  3.14159265f) f->yaw  -= 6.28318531f;
    while (f->yaw  < -3.14159265f) f->yaw  += 6.28318531f;

    forward_vec(f, fwd);
    up_vec(f, up);

    speed = len3(f->vel);
    f->airspeed = speed;

    /* --- hucum acisi ---
     *
     * Hiz vektoru ile burun arasindaki TOPLAM aciyi almak yanlisti: yan
     * kayma (sideslip) da hucum acisina karisiyordu, bu yuzden her donuste
     * sahte STALL uyarisi cikiyordu. Dogrusu hizi govde eksenlerine cevirip
     * yalnizca DIKEY bileseni kullanmaktir. */
    if (speed > 1.0f) {
        float m[3][3];
        float vbx, vby, vbz;

        flight_orientation_matrix(f, m);

        /* v_body = M^T * v_world */
        vbx = m[0][0] * f->vel[0] + m[1][0] * f->vel[1] + m[2][0] * f->vel[2];
        vby = m[0][1] * f->vel[0] + m[1][1] * f->vel[1] + m[2][1] * f->vel[2];
        vbz = m[0][2] * f->vel[0] + m[1][2] * f->vel[1] + m[2][2] * f->vel[2];

        (void)vbx;                  /* yan kayma tasimayi etkilemiyor */
        f->aoa = atan2f(-vby, -vbz);
    } else {
        f->aoa = 0.0f;
    }

    /* Stall uyarisi: kritik aci ANLIK asildiginda degil, bir sure uzerinde
     * kalindiginda verilir. Manevrada aci kisa sureligine sinira degebilir;
     * her dokunusta korna calmasi uyariyi anlamsizlastiriyordu. */
    {
        float crit = STALL_ANGLE_RAD + f->flap * 0.06f;

        if (f->aoa > crit)
            f->stall_timer += dt;
        else
            f->stall_timer -= dt * 2.0f;    /* cikis daha hizli */

        f->stall_timer = clampf(f->stall_timer, 0.0f, 1.0f);
        f->stalled = (f->stall_timer > 0.28f);
    }

    /* Statik kararlilik ve trim.
     *
     * Ucak hucum acisini SIFIRA degil, agirligini tasiyan TRIM acisina
     * dogru cevirir. Sifira cekmek yanlisti: sifir hucum acisinda kanat
     * tasima uretmez, ucak dusup hucum acisini buyutuyor ve sonunda takla
     * atiyordu. Gercek ucakta bu dengeyi kuyruk ve trim kurar; sonuc,
     * kumanda birakildiginda ucagin kendi kendine duz ucmasidir. */
    if (speed > 12.0f) {
        float q = 0.5f * AIR_DENSITY * speed * speed * WING_AREA_M2;
        float cl_needed = (f->mass_kg * GRAVITY) / (q > 1.0f ? q : 1.0f);
        float trim_aoa = (cl_needed - f->flap * CL_FLAP) / CL_SLOPE;
        float k = PITCH_STABILITY * dt;
        float damp = speed / CONTROL_REF_MS;

        trim_aoa = clampf(trim_aoa, -0.05f, STALL_ANGLE_RAD * 0.85f);

        if (damp > 1.2f)
            damp = 1.2f;
        f->pitch -= (f->aoa - trim_aoa) * k * damp;
    }

    /* --- kuvvetler --- */
    thrust = f->throttle * MAX_THRUST_N;
    if (f->fuel_kg <= 0.0f)
        thrust = 0.0f;                      /* yakit bitti: suzulme */

    lift = flight_lift_force(speed, f->aoa, f->flap, f->spoiler);

    /* Kanat yuklemesi: sert donuste tasima agirligin katina cikar.
     * Yapisal sinir asilirsa tasima kirpilir - ucak "cekmiyor". */
    f->g_load = lift / (f->mass_kg * GRAVITY);

    /* Sinir iki yonde de gecerli. Onceden yalnizca pozitif G kirpiliyordu;
     * ters yonde -30 G'ye varan tasima serbest kaliyor ve ucagi firlatiyordu. */
    if (f->g_load > G_LIMIT) {
        lift *= G_LIMIT / f->g_load;
        f->g_load = G_LIMIT;
    } else if (f->g_load < -G_NEG_LIMIT) {
        lift *= -G_NEG_LIMIT / f->g_load;
        f->g_load = -G_NEG_LIMIT;
    }
    drag = flight_drag_force_full(speed, f->aoa, f->flap, f->spoiler,
                                  f->gear_down);

    f->mass_kg = EMPTY_MASS_KG + (f->fuel_kg > 0.0f ? f->fuel_kg : 0.0f);

    for (i = 0; i < 3; i++)
        acc[i] = 0.0f;

    /* itki: burun yonunde */
    for (i = 0; i < 3; i++)
        acc[i] += fwd[i] * thrust / f->mass_kg;

    /* tasima: kanat duzlemine dik (yukari yon) */
    for (i = 0; i < 3; i++)
        acc[i] += up[i] * lift / f->mass_kg;

    /* surukleme: hareketin tersine */
    if (speed > 0.1f) {
        for (i = 0; i < 3; i++)
            acc[i] -= (f->vel[i] / speed) * drag / f->mass_kg;
    }

    /* agirlik */
    acc[1] -= GRAVITY;

    for (i = 0; i < 3; i++) {
        f->vel[i] += acc[i] * dt;
        f->pos[i] += f->vel[i] * dt;
    }

    /* --- tekerlek animasyonu --- */
    {
        float target = f->gear_down ? 1.0f : 0.0f;
        float step = dt * 1.2f;

        if (f->gear_pos < target) {
            f->gear_pos += step;
            if (f->gear_pos > target) f->gear_pos = target;
        } else if (f->gear_pos > target) {
            f->gear_pos -= step;
            if (f->gear_pos < target) f->gear_pos = target;
        }
    }

    /* --- pist yuzeyi ile temas ---
     * Pist alaninin uzerindeysek ve yeterince alcaksak tekerlekler yere
     * basar: dikey hiz kesilir, yatay hizi surtunme yavaslatir. Burun ancak
     * yeterli hizda kalkabilir (rotate hizi), boylece kalkis gercekci olur. */
    {
        float deck_top = DECK_Y + (f->gear_down ? GEAR_HEIGHT : 0.4f);
        float dx = f->pos[0] - f->ground_ref[0];
        float dz = f->pos[2] - f->ground_ref[2];
        int over_runway = flight_over_runway(f, dx, dz);

        f->on_ground = 0;

        if (over_runway && f->pos[1] <= deck_top) {
            float horiz;

            f->pos[1] = deck_top;
            if (f->vel[1] < 0.0f)
                f->vel[1] = 0.0f;
            f->on_ground = 1;

            /* yerdeyken burun asagi gitmez, yatis duzelir */
            if (f->pitch < 0.0f)
                f->pitch = 0.0f;
            f->roll -= f->roll * (dt * 4.0f);

            /* yeterli hiz yoksa burun kalkmaz */
            horiz = sqrtf(f->vel[0] * f->vel[0] + f->vel[2] * f->vel[2]);
            if (horiz < ROTATE_SPEED_MS && f->pitch > 0.06f)
                f->pitch = 0.06f;

            /* Tekerlek surtunmesi ve fren.
             * Surtunme sabit bir yavaslama uretir (mu * g); carpimsal
             * yazilirsa hiza orantili olur ve ucak kalkis hizina hic
             * ulasamaz - ilk yazimda bu hata vardi. */
            {
                float mu = ROLL_FRICTION
                           + ((f->brakes || f->spoiler > 0.5f) ? BRAKE_FRICTION : 0.0f);
                float decel = mu * GRAVITY * dt;
                float horiz = sqrtf(f->vel[0] * f->vel[0] + f->vel[2] * f->vel[2]);

                if (horiz > 0.001f) {
                    float nh = horiz - decel;

                    if (nh < 0.0f)
                        nh = 0.0f;
                    f->vel[0] *= nh / horiz;
                    f->vel[2] *= nh / horiz;
                }
            }
        } else if (f->pos[1] < WATER_SAFE_ALT) {
            /* Deniz yuzeyi: altina inilemez. Su, tekerlekten cok daha fazla
             * direnc gosterir - ucak burada serbestce kayamaz. */
            float horiz = sqrtf(f->vel[0] * f->vel[0] + f->vel[2] * f->vel[2]);

            f->pos[1] = WATER_SAFE_ALT;
            if (f->vel[1] < 0.0f)
                f->vel[1] = 0.0f;
            f->on_ground = 1;

            if (horiz > 0.001f) {
                float nh = horiz - WATER_DRAG * GRAVITY * dt;

                if (nh < 0.0f)
                    nh = 0.0f;
                f->vel[0] *= nh / horiz;
                f->vel[2] *= nh / horiz;
            }
        }

        if (!f->on_ground && f->pos[1] > DECK_Y + 6.0f)
            f->airborne = 1;
    }

    /* --- tekerlek donusu (animasyon icin) ---
     * Yerdeyken tekerlek hizla orantili doner; havadayken yavaslar. */
    {
        float horiz = sqrtf(f->vel[0] * f->vel[0] + f->vel[2] * f->vel[2]);

        if (f->on_ground)
            f->wheel_spin += (horiz / WHEEL_RADIUS_M) * dt;
        else
            f->wheel_spin += (horiz / WHEEL_RADIUS_M) * dt * 0.35f;

        while (f->wheel_spin > 6.28318531f)
            f->wheel_spin -= 6.28318531f;
    }

    /* --- yakit --- */
    f->fuel_kg -= f->throttle * FUEL_BURN_KGS * dt;
    if (f->fuel_kg < 0.0f)
        f->fuel_kg = 0.0f;
}

float flight_speed_kmh(const Flight *f)
{
    return f->airspeed * 3.6f;
}

float flight_altitude(const Flight *f)
{
    return f->pos[1];
}

float flight_fuel_pct(const Flight *f)
{
    return f->fuel_kg / FUEL_FULL_KG * 100.0f;
}
