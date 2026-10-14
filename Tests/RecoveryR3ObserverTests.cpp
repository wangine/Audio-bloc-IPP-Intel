// Additional R3 observer invariants. Synthetic clocks; no device/PCM capture.
#include "EgressClockEstimator.h"
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <thread>
using namespace entropy::egress;
static std::atomic<bool> auditAlloc{false};static std::atomic<unsigned> allocations{0};
void* operator new(std::size_t n){if(auditAlloc.load())++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
static int errors=0;static void check(bool good,const char* msg){if(!good){++errors;std::printf("FAIL %s\n",msg);}}
int main(){
    ClockEstimator e;e.prepare(192000);ClockObservation o{};o.continuous=true;o.renderFrames=1024;o.fillBefore=4096;
    double time=0,phaseError=0,maximumPhaseError=0,finalError=0;ClockEstimate result{};
    for(unsigned n=1;time<160;++n){
        const double rate=192000*(1+(time<48?-180e-6:180e-6));const double dt=1024/rate;time+=dt;
        o.sinkFrame=uint64_t(n)*1024;o.nominalPublished=o.sinkFrame;o.host100ns=10000000+uint64_t(std::llround(time*1e7));
        result=e.observe(o);
        if(result.valid&&time>40){phaseError+=(result.sinkFramesPerSecond-rate)*dt;maximumPhaseError=std::max(maximumPhaseError,std::abs(phaseError));}
        finalError=(result.sinkFramesPerSecond/rate-1)*1e6;
    }
    check(result.valid&&std::abs(finalError)<.1,"real drift step eventually tracked, not frozen");
    check(maximumPhaseError<1600,"bounded integrated observer lag for -180 to +180 ppm step");
    std::printf("synthetic 360 ppm frequency step: integrated observer lag %.3f frames; final error %.6f ppm\n",maximumPhaseError,finalError);
    const auto before=e.quality();check(before.valid&&before.coherent&&before.windowSeconds>31.8,"full window acquired without playback buffering");
    auditAlloc=true;
    for(unsigned n=0;n<3000;++n){o.host100ns+=53333;o.sinkFrame+=1024;o.nominalPublished+=1024;e.observe(o);(void)e.quality();}
    auditAlloc=false;check(allocations==0,"no C++ heap allocation during observe/quality");
    std::atomic<bool> done{false},ready{false};std::atomic<unsigned> coherent{0},bad{0};
    std::thread reader([&]{ready=true;while(!done.load(std::memory_order_acquire)){
        auto q=e.quality();if(q.coherent){++coherent;if(q.valid&&(!std::isfinite(q.filteredHz)||q.filteredHz<96000||q.filteredHz>288000||q.windowSeconds<3.9))++bad;}
    }});
    while(!ready.load())std::this_thread::yield();
    for(unsigned n=0;n<15000;++n){o.host100ns+=53333;o.sinkFrame+=1024;o.nominalPublished+=1024;e.observe(o);}
    done.store(true,std::memory_order_release);reader.join();check(coherent>0&&bad==0,"bounded passive observation with concurrent estimator");
    auto q=e.quality();check(q.observationFrame&&q.observationHost100ns&&q.estimates>100&&q.accepted>1000,"quality metrics available without forensic macro");
    std::printf("allocation calls in audited region=%u; concurrent coherent reads=%u; errors=%d\n",allocations.load(),coherent.load(),errors);return errors?1:0;
}
