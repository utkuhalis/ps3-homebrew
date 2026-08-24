#include <stdio.h>
#include <math.h>
#include "../source/flight.h"
int main(void){
    Flight f; float pos[3]={0,800,0};
    flight_init(&f,pos,0.0f); f.throttle=0.75f;
    for(int i=0;i<60*10;i++) flight_update(&f,1.0f/60.0f);
    printf("duz ucus sonu: hiz=%.1f y=%.1f aoa=%.3f pitch=%.3f\n",f.airspeed,f.pos[1],f.aoa,f.pitch);
    for(int i=0;i<60*30;i++){
        float bt=0.45f;
        f.in_roll=(bt-f.roll)*3.0f-f.r_rate*1.5f;
        if(f.in_roll>1)f.in_roll=1; if(f.in_roll<-1)f.in_roll=-1;
        { float ae=800.0f-f.pos[1], vs=f.vel[1];
          f.in_pitch=ae*0.02f-vs*0.20f-f.p_rate*1.0f;
          if(f.in_pitch>1)f.in_pitch=1; if(f.in_pitch<-1)f.in_pitch=-1; }
        flight_update(&f,1.0f/60.0f);
        if(i%30==0||f.stalled)
            printf("t=%5.1f hiz=%6.1f y=%7.1f roll=%6.3f pitch=%6.3f aoa=%7.3f G=%5.2f stall=%d\n",
                i/60.0f,f.airspeed,f.pos[1],f.roll,f.pitch,f.aoa,f.g_load,f.stalled);
        if(f.stalled&&i>60) { printf("ILK STALL t=%.2f\n", i/60.0f); break; }
    }
    return 0;
}
