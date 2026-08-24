#include <stdio.h>
#include <math.h>
#include "../source/flight.h"
/* aoa hesabini dogrudan sina: burun asagi, hiz yatay -> aoa POZITIF olmali */
static void probe(float pitch, float roll, float vx, float vy, float vz){
    Flight f; float pos[3]={0,1000,0};
    flight_init(&f,pos,0.0f);
    f.pitch=pitch; f.roll=roll; f.yaw=0.0f;
    f.vel[0]=vx; f.vel[1]=vy; f.vel[2]=vz;
    f.throttle=0.0f;
    flight_update(&f,1.0f/6000.0f);   /* cok kucuk adim: durum bozulmasin */
    float hiz_acisi = atan2f(vy, sqrtf(vx*vx+vz*vz));
    printf("pitch=%6.3f roll=%5.2f hiz_acisi=%6.3f  -> aoa=%7.3f  (beklenen ~%6.3f)\n",
           pitch, roll, hiz_acisi, f.aoa, hiz_acisi - pitch);
}
int main(void){
    probe( 0.00f, 0.0f, 0,0,-200);      /* duz ucus */
    probe( 0.20f, 0.0f, 0,0,-200);      /* burun yukari, hiz yatay */
    probe(-0.50f, 0.0f, 0,0,-200);      /* burun asagi, hiz yatay */
    probe(-0.97f, 0.0f, 0,22,-198);     /* trace'teki durum */
    probe(-0.97f, 0.3f, 0,22,-198);     /* yatisli hali */
    return 0;
}
