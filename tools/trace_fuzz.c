#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "../source/flight.h"
int main(void){
    Flight f; float rw[2]={0,0};
    unsigned s=12345; int worst_i=-1; float worst=0;
    flight_init_on_runway(&f, rw, 0.0f, 460.0f);
    f.throttle=1.0f;
    for(int i=0;i<60*600;i++){          /* 10 dakika */
        s = s*1103515245u + 12345u;
        float r1 = ((s>>16)&1023)/511.5f - 1.0f;
        s = s*1103515245u + 12345u;
        float r2 = ((s>>16)&1023)/511.5f - 1.0f;
        if(i%37==0){ f.in_pitch=r1; f.in_roll=r2; }
        if(i%211==0){ f.flap = (float)((s>>8)&3)/3.0f; }
        if(i%307==0){ f.gear_down = (s>>5)&1; }
        if(i%401==0){ f.spoiler = ((s>>3)&1)?1.0f:0.0f; f.brakes=(s>>4)&1; }
        flight_update(&f,1.0f/60.0f);
        if(!(f.airspeed==f.airspeed) || f.airspeed>1e5f){
            printf("IRAKSAMA t=%.1fs hiz=%g y=%g pitch=%g aoa=%g\n",
                   i/60.0f,f.airspeed,f.pos[1],f.pitch,f.aoa);
            return 1;
        }
        if(f.airspeed>worst){worst=f.airspeed;worst_i=i;}
    }
    printf("10 dakika iraksama yok. en yuksek hiz=%.1f m/s (%.0f km/h) t=%.1fs\n",
           worst, worst*3.6f, worst_i/60.0f);
    printf("son: hiz=%.1f y=%.1f aoa=%.3f stall=%d\n",f.airspeed,f.pos[1],f.aoa,f.stalled);
    return 0;
}
