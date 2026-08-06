#ifndef FLIGHT_H
#define FLIGHT_H

/* Ucus modeli: itki, tasima, surukleme, agirlik ve stall.
 *
 * Donanim bagimsizdir (birim testli). Gercek aerodinamik degil, oyun icin
 * sadelestirilmis bir modeldir; ucak gibi hissettirmeyi hedefler. */

#define EMPTY_MASS_KG    3800.0f
#define FUEL_FULL_KG      900.0f
#define MAX_THRUST_N    42000.0f
#define WING_AREA_M2       28.0f
#define GRAVITY             9.81f

#define STALL_ANGLE_RAD     0.28f   /* ~16 derece */
#define WATER_SAFE_ALT       2.0f   /* deniz seviyesinin uzerinde kalinacak */

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

    /* son hesaplanan degerler - HUD ve ses icin */
    float airspeed;             /* m/s */
    float aoa;                  /* hucum acisi, radyan */
    int   stalled;
    float mass_kg;
    int   on_ground;
} Flight;

void  flight_init(Flight *f, const float start_pos[3], float start_yaw);
void  flight_update(Flight *f, float dt);

/* Tureyen degerler */
float flight_speed_kmh(const Flight *f);
float flight_altitude(const Flight *f);
float flight_fuel_pct(const Flight *f);

/* Belirli bir durumda uretilen tasima kuvveti (test ve ayar icin acik) */
float flight_lift_force(float airspeed, float aoa, float flap, float spoiler);
float flight_drag_force(float airspeed, float aoa, float flap, float spoiler);

#endif
