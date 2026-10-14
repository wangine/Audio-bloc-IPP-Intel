// Deterministic observer fixture; synthetic clocks, NOT captured DAC PCM.
#include "EgressClockEstimator.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <atomic>
using namespace entropy::egress;
static int failures=0;
static void check(bool b,const char* what){if(!b){std::printf("FAIL %s\n",what);++failures;}}
struct Trial {double maximumErrorPpm=0,finalErrorPpm=0,firstValid=0;bool lossOfValidity=false;};
static Trial run(unsigned block,double drift,bool perturb,bool stalledProducer=false,uint64_t anchor=0) {
    ClockEstimator e;check(e.prepare(192000),"prepare");Trial t;const double trueRate=192000*(1+drift*1e-6);bool once=false;
    // A block-quantized clock paired with a coarse 1 ms timestamp; after 48 s
    // the timestamp phase steps by 1/3 ms. This is a controlled sensitivity
    // probe inspired by, not reconstructed from, the 5 Hz user snapshots.
    for(uint64_t n=1;double(n*block)/trueRate<90;++n){
        const uint64_t f=n*block;const double time=double(f)/trueRate;
        double observed=time;
        if(perturb)observed=std::ceil((time-1e-10)*1000.0)*.001 + (time>=48 ? -1.0/3000.0 : 0.0);
        ClockObservation o{anchor+f,anchor+10000000+uint64_t(std::llround(observed*1e7)),
            stalledProducer?1000u:anchor+f,block,4096,true};
        auto r=e.observe(o);
        if(r.valid){if(!once){t.firstValid=time;once=true;}
            const double error=(r.sinkFramesPerSecond/trueRate-1)*1e6;
            if(time>=45)t.maximumErrorPpm=std::max(t.maximumErrorPpm,std::abs(error));
            t.finalErrorPpm=error;
        }else if(once)t.lossOfValidity=true;
    }
    return t;
}
int main(){
    for(unsigned b:{256u,1024u,2048u}){
        const auto t=run(b,0,true);
        std::printf("phase step + coarse timestamps block=%u peak_error_ppm=%.6f final=%.6f acquisition=%.3f\n",b,t.maximumErrorPpm,t.finalErrorPpm,t.firstValid);
        check(t.maximumErrorPpm<20,"timestamp phase noise must not command >20 ppm after acquisition");
        check(t.firstValid>=3.9&&t.firstValid<5.0,"acquisition remains about four seconds, not 32 seconds");
        check(!t.lossOfValidity,"observer continuous after acquisition");
    }
    for(double ppm:{-180.0,-70.0,0.0,100.0,180.0}){
        const auto t=run(1024,ppm,false);
        check(std::abs(t.finalErrorPpm)<.01,"preserve real constant oscillator drift");
    }
    const auto stalled=run(1024,70,false,true),reference=run(1024,70,false);
    check(stalled.finalErrorPpm==reference.finalErrorPpm,"publication cadence never steers the clock");
    const auto large=run(1024,70,false,false,UINT64_C(18014398509481984));
    check(std::abs(large.finalErrorPpm)<.01,"integer anchors before FP conversion for long uptime");
    ClockEstimator e;check(e.prepare(192000),"prepare reset");ClockObservation o{};o.renderFrames=1024;o.fillBefore=4096;o.continuous=true;
    for(unsigned n=1;n<=1200;++n){o.sinkFrame=n*1024;o.host100ns=10000000+uint64_t(std::llround(n*1024.0/192000*1e7));o.nominalPublished=n*1024;e.observe(o);}
    auto bad=o;bad.host100ns--;check(!e.observe(bad).valid,"nonmonotonic rejected");
    o.host100ns+=6000000;o.sinkFrame+=115200;o.nominalPublished+=115200;check(!e.observe(o).valid,"gap over 500 ms reacquires, not fake valid");
    o.continuous=false;check(!e.observe(o).valid,"explicit discontinuity invalidates estimate");
    check(!e.prepare(0),"invalid configuration rejected");
    std::printf("failures=%d\n",failures);return failures?1:0;
}
