#include "RealtimeFir.h"
#include "ProgramEpochTrace.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <type_traits>
#include <vector>
// Representation requirement: no per-partition complex/IPP bridge in processing.
static_assert(std::is_same_v<decltype(entropy::standalone::FirKernel{}.tail)::value_type::value_type,Ipp64fc>);
int main(){
    using namespace entropy::standalone;int failures=0;FixedFFT fft;
    for(int taps:{2049,4097,8193})for(int warm:{1,7,31,127,128,129,511,8191,8320,17001}){
        auto kernel=std::make_unique<FirKernel>();std::vector<double> h(taps);h[0]=0.7;h[127]=-0.2;h[128]=0.3;h[taps-1]=0.1;kernel->build(h.data(),taps,fft);
        auto old=std::make_unique<RealtimeFir>(),fresh=std::make_unique<RealtimeFir>();std::vector<double> poison(warm,0.37);old->process(poison.data(),warm,kernel.get(),nullptr,1,0);
        for(int round=0;round<3;++round){old->reset();fresh->reset();std::vector<double> a(taps+1024),b(a.size());a[0]=b[0]=0.25;
            for(int at=0;at<int(a.size());){int n=std::min(31,int(a.size())-at);old->process(a.data()+at,n,kernel.get(),nullptr,1,0);fresh->process(b.data()+at,n,kernel.get(),nullptr,1,0);at+=n;}
            for(size_t i=0;i<a.size();++i)if(a[i]!=b[i]||!std::isfinite(a[i])){++failures;break;}
            std::fill(poison.begin(),poison.end(),-0.42);old->process(poison.data(),warm,kernel.get(),nullptr,1,0);
        }
    }
    FirEpochTrace epochs;for(int i=0;i<20000;++i)epochs.process(99999,8192,0,false,1);epochs.reset();
    for(int i=0;i<17000;++i){auto token=epochs.process(i+1,8192,0,false,1);if(token!=(i<8192?0:uint64_t(i-8192+1)))++failures;}
    std::printf("Stage03 reset fresh-state/epoch regression failures=%d\n",failures);return failures?1:0;
}
