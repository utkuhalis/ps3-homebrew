#include <stdio.h>
#include <math.h>
#include "../source/flight.h"
int main(void){
    Flight f; float rw[2]={0,0};
    /* take off, then dive towards the water - the scenario the user hit */
    flight_init_on_runway(&f, rw, 0.0f, 460.0f);
    f.throttle=1.0f;
    for(int i=0;i<60*20;i++){ if(f.airspeed>ROTATE_SPEED_MS) f.in_pitch=0.6f; flight_update(&f,1.0f/60.0f); }
    printf("after takeoff: speed=%.1f y=%.1f aoa=%.3f stall=%d\n", f.airspeed, f.pos[1], f.aoa, f.stalled);
    f.in_pitch=-1.0f;      /* push the nose down */
    for(int i=0;i<60*60;i++){
        flight_update(&f,1.0f/60.0f);
        if(i%300==0) printf("t=%5.1fs speed=%12.1f y=%9.2f aoa=%7.3f pitch=%7.3f stall=%d G=%6.2f ground=%d\n",
            20+i/60.0f, f.airspeed, f.pos[1], f.aoa, f.pitch, f.stalled, f.g_load, f.on_ground);
    }
    printf("END: speed=%.1f km/h\n", f.airspeed*3.6f);
    return 0;
}
