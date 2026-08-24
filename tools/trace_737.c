#include <stdio.h>
#include <math.h>
#include "../source/flight.h"
int main(void){
    Flight f; float pos[3]={0,3000,0};
    flight_init(&f,pos,0.0f); f.throttle=0.80f;
    for(int i=0;i<60*10;i++) flight_update(&f,1.0f/60.0f);
    printf("duz: hiz=%.1f y=%.1f aoa=%.3f pitch=%.3f\n",f.airspeed,f.pos[1],f.aoa,f.pitch);
    for(int i=0;i<60*16;i++){
        float bt=0.30f;
        f.in_roll=(bt-f.roll)*6.0f-f.r_rate*3.0f;
        if(f.in_roll>1)f.in_roll=1; if(f.in_roll<-1)f.in_roll=-1;
        float ae=3000.0f-f.pos[1], vs=f.vel[1];
        f.in_pitch=ae*0.0015f-vs*0.04f-f.p_rate*2.0f;
        if(f.in_pitch>1)f.in_pitch=1; if(f.in_pitch<-1)f.in_pitch=-1;
        flight_update(&f,1.0f/60.0f);
        if(i>=60*7 && i<=60*13 && i%15==0)
            printf("t=%5.2f in_p=%6.3f p_rate=%6.3f pitch=%7.3f aoa=%7.3f vy=%7.2f y=%7.1f hiz=%6.1f L/W=%5.2f\n",
                i/60.0f,f.in_pitch,f.p_rate,f.pitch,f.aoa,f.vel[1],f.pos[1],f.airspeed,f.g_load);
    }
    return 0;
}
