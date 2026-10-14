// Real production ratio law; a renamed R2 implementation is provided by the runner.
#include "EgressRatioController.h"
#include "R2Ratio.h"
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <atomic>
using namespace entropy::egress;
static int fails=0;
static void check(bool ok,const char* what){if(!ok){std::printf("FAIL %s\n",what);++fails;}}
static bool same(double a,double b){return std::bit_cast<std::uint64_t>(a)==std::bit_cast<std::uint64_t>(b);}
int main(){
    EntropyEgressClockConfig c{};c.struct_size=sizeof(c);c.engine_rate=c.sink_rate=192000;
    c.target_fill_frames=4096;c.maximum_correction_ppm=200;c.correction_smoothing_ms=100;
    RatioController r;R2RatioController old;check(r.prepare(c)&&old.prepare(c),"prepare");
    ClockObservation o{};o.continuous=true;o.host100ns=10000000;o.sinkFrame=1024;o.renderFrames=1024;o.fillBefore=4096;
    ClockEstimate e{};e.valid=true;e.sinkFramesPerSecond=192000;
    double previous=0,maxDelta=0;unsigned steps=0;
    for(unsigned n=0;n<50000;++n){
        o.host100ns+=n%23==0 ? 120000:53333;o.sinkFrame+=1024;o.nominalPublished+=1024;
        o.fillBefore=4096+800*std::sin(n*.07);e.sinkFramesPerSecond=192000*(1+180e-6*std::sin(n*.013));
        e.valid=n%29!=0;o.continuous=n%113!=0;
        if(n%997==0){r.reset();old.reset();previous=0;}
        const auto a=r.update(o,e);const auto b=old.update(o,e);
        check(same(a.appliedPpm,b.appliedPpm)&&same(a.requestedPpm,b.requestedPpm)
            &&same(a.stepBegin,b.stepBegin)&&same(a.stepEnd,b.stepEnd)&&a.recoveryRequired==b.recoveryRequired,"ratio bit equality to R2");
        maxDelta=std::max(maxDelta,std::abs(a.appliedPpm-previous));previous=a.appliedPpm;++steps;
        const auto q=r.quality();check(q.coherent&&same(q.appliedPpm,a.appliedPpm)&&same(q.requestedPpm,a.requestedPpm),"owner telemetry");
    }
    auto q=r.quality();check(q.calls==steps&&same(q.maxAbsDeltaApplied,maxDelta),"actual callback max, no 5 Hz reconstruction");
    check(q.maxAbsPpmPerSecond<=50.0000001,"slew law unchanged");
    // Passive observation is safe under a concurrent owner. Do not reset/prepare
    // while the owner executes; this checks only the new observation channel.
    std::atomic<bool> done{false};std::atomic<unsigned> coherent{0},bad{0};
    std::thread reader([&]{while(!done.load(std::memory_order_acquire)){
        auto v=r.quality();if(v.coherent){++coherent;if(!std::isfinite(v.appliedPpm)||std::abs(v.appliedPpm)>200.001||v.maxAbsDeltaApplied+1e-12<std::abs(v.deltaApplied))++bad;}
    }});
    for(unsigned n=0;n<50000;++n){o.host100ns+=53333;o.sinkFrame+=1024;e.valid=true;o.continuous=true;r.update(o,e);}
    done.store(true,std::memory_order_release);reader.join();
    check(coherent>0&&bad==0,"concurrent ratio telemetry");
    std::printf("R2/R3 identical decisions=%u; max_delta=%.9f; observed_slew=%.9f; failures=%d\n",steps,q.maxAbsDeltaApplied,q.maxAbsPpmPerSecond,fails);
    return fails?1:0;
}
