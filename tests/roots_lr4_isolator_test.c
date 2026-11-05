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
    roots_lr4_init(&s);
    for(i=0;i<1000;i++){x=(int32_t)(i*7919u)-3950000;CHECK(roots_lr4_process(&s,x)==x,"bypass exact");}
    s.enabled=1;
    for(i=0;i<4;i++) s.gain[i]=0;
    for(i=0;i<1000;i++) CHECK(roots_lr4_process(&s,(i&1)?50000:-50000)==0,"all mute");
    roots_lr4_init(&s); s.enabled=1;
    for(i=0;i<4;i++) s.gain[i]=32768;
    for(i=0;i<100000;i++) y=roots_lr4_process(&s,10000);
    CHECK(abs(y-10000)<=5,"unity DC");
    {
        const double f[]={20,30,60,120,240,350,700,1000,2500,5000,10000,20000};
        for(i=0;i<sizeof(f)/sizeof(f[0]);i++) {
            double a=measure(f[i],15);
            CHECK(a>0.985 && a<1.015,"unity sum frequency response +/-1.5%");
        }
    }
    for(i=1;i<16;i++) {
        double a=measure(997.0,(int)i);
        CHECK(a>=0.0 && a<1.8,"all 15 band masks remain bounded");
    }
    roots_lr4_init(&s); s.enabled=1;
    for(i=0;i<100000;i++){x=(i&1)?131071:-131071;y=roots_lr4_process(&s,x);
        CHECK(y<=ROOTS_LR4_MAX && y>=-ROOTS_LR4_MAX,"full scale bounded");}
    puts("ROOTS DUB LR4: bypass, mute, DC, response, 15 masks and stress OK");
    return 0;
}
