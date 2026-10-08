#pragma once
#include <ipp.h>
#include <algorithm>
#include <array>
#include <complex>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

namespace entropy::native {
// All allocation, IPP dispatcher initialization, and representative warmup happen
// in construction. Each instance owns its workspace and must not run concurrently.
class IppComplexFft {
    struct Free { void operator()(Ipp8u* p) const noexcept { if(p) ippsFree(p); } };
    using Storage=std::unique_ptr<Ipp8u,Free>;
    Storage specification,work;
    IppsFFTSpec_C_64fc* plan=nullptr;
    int length;
    static void checked(IppStatus status) { if(status!=ippStsNoErr) throw std::runtime_error("IPP FP64 FFT initialization failed"); }
    static Storage allocate(int size) {
        if(size<=0) return {};
        Storage result(ippsMalloc_8u(size));
        if(!result) throw std::bad_alloc();
        return result;
    }
public:
    explicit IppComplexFft(int order):length(order>=1&&order<=24?(1<<order):0) {
        if(!length) throw std::invalid_argument("Unsupported IPP FFT order");
        checked(ippInit());
        int specSize=0,initSize=0,workSize=0;
        checked(ippsFFTGetSize_C_64fc(order,IPP_FFT_DIV_INV_BY_N,ippAlgHintAccurate,&specSize,&initSize,&workSize));
        specification=allocate(specSize);auto init=allocate(initSize);work=allocate(workSize);
        checked(ippsFFTInit_C_64fc(&plan,order,IPP_FFT_DIV_INV_BY_N,ippAlgHintAccurate,specification.get(),init.get()));
        std::vector<Ipp64fc> zero(static_cast<std::size_t>(length));
        checked(ippsFFTFwd_CToC_64fc_I(zero.data(),plan,work.get()));
        checked(ippsFFTInv_CToC_64fc_I(zero.data(),plan,work.get()));
        std::array<Ipp64fc,129> a{},b{},c{};checked(ippsAddProduct_64fc(a.data(),b.data(),c.data(),129));
        double x[128]{},dot=0;checked(ippsDotProd_64f(x,x,128,&dot));
    }
    IppComplexFft(const IppComplexFft&)=delete;
    IppComplexFft& operator=(const IppComplexFft&)=delete;
    void run(Ipp64fc* data,bool inverse=false) const noexcept {
        if(inverse) (void)ippsFFTInv_CToC_64fc_I(data,plan,work.get());
        else (void)ippsFFTFwd_CToC_64fc_I(data,plan,work.get());
    }
    int size() const noexcept { return length; }
};
}

namespace entropy::standalone {
class FixedFFT {
    entropy::native::IppComplexFft plan{8};
    mutable std::array<Ipp64fc,256> bridge{};
public:
    using C=std::complex<double>;
    static constexpr int n=256;
    // std::complex<double> has the standard contiguous real/imag double layout.
    // memcpy avoids strict-aliasing assumptions between it and the IPP struct.
    void run(C* a,bool inverse=false) const noexcept {
        static_assert(sizeof(C)==sizeof(Ipp64fc));
        std::memcpy(bridge.data(),a,sizeof(bridge));plan.run(bridge.data(),inverse);
        std::memcpy(a,bridge.data(),sizeof(bridge));
    }
    void runNative(Ipp64fc* a,bool inverse=false) const noexcept { plan.run(a,inverse); }
};
struct alignas(64) FirKernel {
    static constexpr int maximumTaps=128+64*128;
    std::array<double,128> head{};
    alignas(64) std::array<double,128> reverseHead{};
    std::array<std::array<Ipp64fc,256>,64> tail{};
    int partitions=0;
    int firstNonzeroTap=-1;
    void build(const double* taps,int count,const FixedFFT& fft) noexcept {
        count=taps?std::clamp(count,0,maximumTaps):0;
        firstNonzeroTap=-1;
        for(int i=0;i<count;++i) if(taps[i]!=0.0){firstNonzeroTap=i;break;}
        head.fill(0);for(int i=0;i<std::min(count,128);++i)head[i]=taps[i];
        for(int i=0;i<128;++i)reverseHead[i]=head[127-i];
        partitions=std::max(0,(count-128+127)/128);
        for(int p=0;p<partitions;++p){auto& a=tail[p];a.fill({});for(int j=0;j<128;++j){const int k=128+p*128+j;if(k<count)a[j]={taps[k],0};}fft.runNative(a.data());}
    }
};
// 128-sample direct head and 128-sample overlap-add tail. Tail calculations are
// ready exactly when tap 128 becomes audible: no added partition/group delay.
class alignas(64) RealtimeFir {
    FixedFFT fft;
    alignas(64) std::array<double,256> headHistory{};
    alignas(64) std::array<double,128> chunk{},tailNew{},tailOld{},overlapNew{},overlapOld{};
    std::array<std::array<Ipp64fc,256>,65> history{};
    std::array<Ipp64fc,256> sum{};
    int position=0,newest=64,validPartitions=0;
    const FirKernel* cachedNew=nullptr;const FirKernel* cachedOld=nullptr;
    void evaluate(const FirKernel* k,std::array<double,128>& out,std::array<double,128>& overlap,bool resetOverlap) noexcept {
        out.fill(0);if(!k){overlap.fill(0);return;}
        auto spectrum=[&](int lag){
            sum.fill({});
            for(int p=0;p<k->partitions;++p){const auto& x=history[(newest-p-lag+130)%65];
                // Unwritten slots are logical zero: never read pre-reset spectra.
                if(p+lag>=validPartitions)continue;
                (void)ippsAddProduct_64fc(x.data(),k->tail[p].data(),sum.data(),129);
            }
            for(int b=1;b<128;++b)sum[256-b]={sum[b].re,-sum[b].im};
            fft.runNative(sum.data(),true);
        };
        if(resetOverlap){spectrum(1);for(int i=0;i<128;++i)overlap[i]=sum[i+128].re;}
        spectrum(0);for(int i=0;i<128;++i){out[i]=sum[i].re+overlap[i];overlap[i]=sum[i+128].re;}
    }
    double dot(const FirKernel* k) const noexcept {
        if(!k)return 0;double value=0;
        (void)ippsDotProd_64f(headHistory.data()+position+1,k->reverseHead.data(),128,&value);
        return value;
    }
public:
    // Fixed 7 KiB head/overlap workspace; no clearing proportional to FIR length.
    // The 260 KiB spectral ring is invalidated in O(1), without generation wrap.
    void reset() noexcept {headHistory.fill(0);chunk.fill(0);tailNew.fill(0);tailOld.fill(0);overlapNew.fill(0);overlapOld.fill(0);position=0;newest=64;validPartitions=0;cachedNew=cachedOld=nullptr;}
    void process(double* data,int count,const FirKernel* current,const FirKernel* previous,double mixStart,double mixStep) noexcept {
        if(cachedNew!=current){evaluate(current,tailNew,overlapNew,true);cachedNew=current;}
        if(cachedOld!=previous){evaluate(previous,tailOld,overlapOld,true);cachedOld=previous;}
        for(int i=0;i<count;++i){
            const double input=data[i];headHistory[position]=headHistory[position+128]=input;chunk[position]=input;
            double a=tailNew[position],b=tailOld[position];a+=dot(current);if(previous)b+=dot(previous);
            const double t=std::min(1.0,mixStart+i*mixStep);data[i]=previous?b+(a-b)*t:a;
            if(++position==128){position=0;newest=(newest+1)%65;auto& h=history[newest];h.fill({});for(int j=0;j<128;++j)h[j]={chunk[j],0};fft.runNative(h.data());validPartitions=std::min(65,validPartitions+1);evaluate(current,tailNew,overlapNew,false);if(previous)evaluate(previous,tailOld,overlapOld,false);}
        }
    }
};
}

