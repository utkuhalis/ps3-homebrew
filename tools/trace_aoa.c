#include <stdio.h>
#include <math.h>
#include "../source/flight.h"
/* exercise the aoa computation directly: nose down, velocity level -> aoa must be POSITIVE */
static void probe(float pitch, float roll, float vx, float vy, float vz){
    Flight f; float pos[3]={0,1000,0};
    flight_init(&f,pos,0.0f);
    f.pitch=pitch; f.roll=roll; f.yaw=0.0f;
    f.vel[0]=vx; f.vel[1]=vy; f.vel[2]=vz;
    f.throttle=0.0f;
    flight_update(&f,1.0f/6000.0f);   /* a very small step: do not disturb the state */
    float hiz_acisi = atan2f(vy, sqrtf(vx*vx+vz*vz));
    printf("pitch=%6.3f roll=%5.2f vel_angle=%6.3f  -> aoa=%7.3f  (expected ~%6.3f)\n",
           pitch, roll, hiz_acisi, f.aoa, hiz_acisi - pitch);
}
int main(void){
    probe( 0.00f, 0.0f, 0,0,-200);      /* level flight */
    probe( 0.20f, 0.0f, 0,0,-200);      /* nose up, velocity level */
    probe(-0.50f, 0.0f, 0,0,-200);      /* nose down, velocity level */
    probe(-0.97f, 0.0f, 0,22,-198);     /* the state from the trace */
    probe(-0.97f, 0.3f, 0,22,-198);     /* banked version */
    return 0;
}
