#include <stdio.h>
#include <math.h>
#include "../source/flight.h"
int main(void){
    Flight f; float rw[2]={0,0};
    flight_init_on_runway(&f, rw, 0.0f, 500.0f);
    f.throttle=1.0f;
    for(int i=0;i<60*40;i++){
        float horiz=sqrtf(f.vel[0]*f.vel[0]+f.vel[2]*f.vel[2]);
        f.in_pitch = (horiz>ROTATE_SPEED_MS)?1.0f:0.0f;
        flight_update(&f,1.0f/60.0f);
        if(i%120==0||i==60*40-1){
            float d=sqrtf((f.pos[0]-rw[0])*(f.pos[0]-rw[0])+(f.pos[2]-rw[2])*(f.pos[2]-rw[2]));
            printf("t=%4.1fs hiz=%6.1f pitch=%6.3f aoa=%6.3f y=%7.2f mesafe=%6.1f yer=%d hava=%d L=%7.0f W=%7.0f\n",
                i/60.0f, sqrtf(f.vel[0]*f.vel[0]+f.vel[1]*f.vel[1]+f.vel[2]*f.vel[2]),
                f.pitch, f.aoa, f.pos[1], d, f.on_ground, f.airborne,
                flight_lift_force(f.airspeed,f.aoa,f.flap,f.spoiler), f.mass_kg*9.81f);
        }
    }
    return 0;
}
