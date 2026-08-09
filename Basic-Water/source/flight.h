#ifndef FLIGHT_H
#define FLIGHT_H

/* Ucus modeli: itki, tasima, surukleme, agirlik ve stall.
 *
 * Donanim bagimsizdir (birim testli). Gercek aerodinamik degil, oyun icin
 * sadelestirilmis bir modeldir; ucak gibi hissettirmeyi hedefler. */

#define EMPTY_MASS_KG    3800.0f
#define FUEL_FULL_KG      900.0f
/* Itki: 4700 kg'lik bir is jeti icin T/W ~ 0.45. Ilk degerde (42 kN) oran
 * 0.91 idi - savas ucagi seviyesi; ucak oyuncak gibi firliyordu. */
#define MAX_THRUST_N    24000.0f
#define WING_AREA_M2       28.0f
#define GRAVITY             9.81f

#define STALL_ANGLE_RAD     0.28f   /* ~16 derece */
#define WATER_SAFE_ALT       2.0f   /* deniz seviyesinin uzerinde kalinacak */

/* Pist yuzeyi ve yer teması */
#define DECK_Y               9.0f   /* pist platformunun yuzeyi */
#define GEAR_HEIGHT          1.6f   /* tekerlekler acikken govde yuksekligi */
#define ROLL_FRICTION        0.030f /* tekerlek surtunmesi */
#define BRAKE_FRICTION       0.240f /* fren (spoiler ile birlikte) */
#define ROTATE_SPEED_MS     58.0f   /* bu hizin uzerinde burun kalkabilir */

/* Acisal atalet: kumanda bir MOMENT uretir, ucak ona ivmelenerek uyar.
 * Once girdi dogrudan aciyi degistiriyordu; ucak kagit gibi aninda donuyor,
 * kumanda birakilinca aninda duruyordu. Bu zaman sabitleri ucagin agirlik
 * hissini belirler: buyudukce ucak agirlasir. */
#define PITCH_INERTIA_S   1.15f
#define ROLL_INERTIA_S    0.55f
#define YAW_INERTIA_S     1.60f

/* Manevra sirasinda tasima artar, indüklenen surukleme de artar: sert
 * donuste ucak hiz kaybeder. */
#define G_LIMIT           3.2f
#define G_NEG_LIMIT       1.6f   /* ters yonde yapisal sinir daha dusuktur */
#define WATER_DRAG        1.35f  /* su yuzeyinde surtunme katsayisi */

/* Kumanda etkinligi dinamik basincla artar: bu hizin altinda kumandalar
 * agirlasir, ustunde tam etkilidir. Gercek ucakta oldugu gibi durur ucakta
 * yon degistirmek imkansizdir. */
#define CONTROL_REF_MS      85.0f
#define GEAR_DRAG            0.055f
#define WHEEL_RADIUS_M       0.42f  /* tekerlek yaricapi (donme animasyonu) */

/* Pist yuzeyi olculeri (runway.h ile ayni olmali). Ucus modulu cizim
 * modulune bagimli olmasin diye burada tekrar tanimlanir; test bu ikisinin
 * ayni kaldigini dogrular. */
#define RUNWAY_HALF_LEN    550.0f
#define RUNWAY_HALF_WID     48.0f /* acik tekerleklerin surukleme katkisi */

/* Kumanda yuzeyi sinirlari (radyan) */
#define FLAP_MAX_RAD     0.70f      /* ~40 derece */
#define SPOILER_MAX_RAD  0.96f      /* ~55 derece */
#define AILERON_MAX_RAD  0.35f
#define ELEVATOR_MAX_RAD 0.44f
#define RUDDER_MAX_RAD   0.44f

typedef struct {
    float pos[3];
    float vel[3];

    float yaw, pitch, roll;     /* radyan */

    float throttle;             /* 0..1 */
    float fuel_kg;

    /* kumanda girdileri, -1..+1 (flap ve spoiler 0..1) */
    float in_pitch, in_roll, in_yaw;
    float flap, spoiler;
    int   gear_down;            /* 1: tekerlekler acik */
    float gear_pos;             /* 0..1 animasyon konumu */
    int   brakes;               /* yerde fren */

    /* acisal hizlar (radyan/saniye) - atalet bunlar uzerinden calisir */
    float p_rate, r_rate, y_rate;

    /* son hesaplanan degerler - HUD ve ses icin */
    float airspeed;             /* m/s */
    float aoa;                  /* hucum acisi, radyan */
    int   stalled;
    float stall_timer;          /* kritik acinin ne kadar suredir uzerinde */
    float mass_kg;
    float g_load;               /* kanat yuklemesi, 1.0 = duz ucus */
    float wheel_spin;           /* tekerlek donme acisi (radyan) */
    int   on_ground;            /* pist yuzeyinde mi */
    int   airborne;             /* bir kez havalandi mi (kalkis yapildi) */

    /* Yer temasinin gecerli oldugu PIST DIKDORTGENI.
     * Onceden yaricapi olan bir daire kullaniliyordu; pist 1100x96 birim
     * oldugu icin ucak pistin yanindaki denize de asfalta iner gibi
     * konuyordu. */
    float ground_ref[3];        /* pist merkezi */
    float ground_heading;       /* pist yonu (radyan) */
    float ground_half_len;      /* yaridan uzunluk */
    float ground_half_wid;      /* yaridan genislik */
} Flight;

/* Havada baslatir (serbest ucus) */
void  flight_init(Flight *f, const float start_pos[3], float start_yaw);

/* Pist uzerinde, motor rolantide, tekerlekler acik olarak baslatir.
 * Kalkisi oyuncu yapar: gaz ver, hizlan, ROTATE_SPEED_MS uzerinde burnu kaldir. */
/* start_offset: pist merkezinden geriye (kuyruk yonune) kaydirma; ucagin
 * pist basindan kalkis kosusuna baslamasi icin. */
void  flight_init_on_runway(Flight *f, const float runway_xz[2], float heading,
                            float start_offset);

float flight_drag_force_full(float airspeed, float aoa, float flap,
                             float spoiler, int gear_down);
void  flight_update(Flight *f, float dt);

/* Pist dikdortgeni testi (dx,dz pist merkezine gore) */
int   flight_over_runway(const Flight *f, float dx, float dz);

/* Govde koordinatini dunyaya tasir (model cizimi ve kamera yerlesimi) */
void  flight_body_to_world(const Flight *f, const float in[3], float out[3]);

/* Yonelim matrisi: body_to_world'un oteleme disindaki kismi */
void  flight_orientation_matrix(const Flight *f, float m[3][3]);

/* Tureyen degerler */
float flight_speed_kmh(const Flight *f);
float flight_altitude(const Flight *f);
float flight_fuel_pct(const Flight *f);

/* Belirli bir durumda uretilen tasima kuvveti (test ve ayar icin acik) */
float flight_lift_force(float airspeed, float aoa, float flap, float spoiler);
float flight_drag_force(float airspeed, float aoa, float flap, float spoiler);

#endif
