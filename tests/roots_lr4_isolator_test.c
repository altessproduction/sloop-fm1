/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "roots_lr4_isolator.h"
#define CHECK(c,m) do { if (!(c)) { fprintf(stderr,"FAIL: %s\n",m); return 1; } } while(0)

static double measure(double hz, int mask)
{
    roots_lr4_t s; unsigned n; double si=0,co=0;
    roots_lr4_init(&s); s.enabled=1;
    for(n=0;n<4;n++) s.gain[n]=(mask&(1u<<n))?32768:0;
    for(n=0;n<44100*2;n++) {
        double a=sin(2.0*3.141592653589793*hz*n/44100.0);
        int32_t y=roots_lr4_process(&s,(int32_t)(a*50000.0));
        if(n>=44100) { double p=2.0*3.141592653589793*hz*n/44100.0;
            si+=(double)y*sin(p); co+=(double)y*cos(p); }
    }
    return 2.0*hypot(si,co)/(44100.0*50000.0);
}
int main(void)
{
    roots_lr4_t s; unsigned i; int32_t x,y;
    const double f[]={20,30,60,120,240,350,700,1000,2500,5000,10000,20000};
    CHECK(sizeof(roots_lr4_t)<=264u,"per-channel state stays within 264 bytes");
    roots_lr4_init(&s);
    for(i=0;i<1000;i++){x=(int32_t)(i*7919u)-3950000;CHECK(roots_lr4_process(&s,x)==x,"bypass exact");}
    s.enabled=1;
    for(i=0;i<4;i++) s.gain[i]=0;
    for(i=0;i<64;i++) (void)roots_lr4_process(&s,(i&1)?50000:-50000);
    CHECK(s.wet==32768u,"enable ramp reaches fully wet in 64 samples");
    for(i=0;i<1000;i++) CHECK(roots_lr4_process(&s,(i&1)?50000:-50000)==0,"all mute");
    s.enabled=0;
    for(i=0;i<64;i++) (void)roots_lr4_process(&s,(i&1)?50000:-50000);
    CHECK(s.wet==0u && !s.running,"disable ramp returns to exact bypass");
    x=12345;
    CHECK(roots_lr4_process(&s,x)==x,"bypass remains bit-identical after disable");
    s.enabled=1;
    for(i=0;i<64;i++) (void)roots_lr4_process(&s,10000);
    CHECK(s.wet==32768u && s.running,"re-enable restarts filter state and fades in");
    roots_lr4_init(&s); s.enabled=1;
    for(i=0;i<4;i++) s.gain[i]=32768;
    for(i=0;i<100000;i++) y=roots_lr4_process(&s,10000);
    CHECK(abs(y-10000)<=25,"unity DC within fixed-point Q8 rounding tolerance (0.25 %)");
    {
        for(i=0;i<sizeof(f)/sizeof(f[0]);i++) {
            double a=measure(f[i],15);
            CHECK(a>0.995 && a<1.005,"phase-aligned unity sum response within +/-0.5%");
        }
    }
    CHECK(measure(20,1)>0.97 && measure(1000,1)<0.02,"SUB is low-passed below 120 Hz");
    CHECK(measure(120,2)>0.45 && measure(120,2)<0.55
       && measure(350,2)>0.45 && measure(350,2)<0.55,"BASS is bounded by the 120/350 Hz crossovers");
    CHECK(measure(1000,4)>0.90 && measure(2500,4)>0.45 && measure(2500,4)<0.55,"MIDS occupies the 350 Hz..2.5 kHz band");
    CHECK(measure(10000,8)>0.95 && measure(1000,8)<0.05,"TOPS is high-passed above 2.5 kHz");
    for(i=1;i<16;i++) {
        unsigned k;
        for(k=0;k<sizeof(f)/sizeof(f[0]);k++) {
            double a=measure(f[k],(int)i);
            CHECK(a>=0.0 && a<1.02,"all 15 band masks remain bounded across the audio band");
        }
    }
    roots_lr4_init(&s); s.enabled=1;
    for(i=0;i<100000;i++){x=(i&1)?ROOTS_LR4_MAX:-ROOTS_LR4_MAX;y=roots_lr4_process(&s,x);
        CHECK(y<=ROOTS_LR4_MAX && y>=-ROOTS_LR4_MAX,"full scale bounded");}
    puts("ROOTS DUB LR4: bypass, mute, DC, response, 15 masks and stress OK");
    return 0;
}
