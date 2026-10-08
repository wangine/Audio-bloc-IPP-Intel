#include "RealtimeFir.h"
#include "IppSpectrum.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <vector>
#include <atomic>
#include <malloc.h>
static std::atomic<unsigned long long> allocations{0};
void* operator new(std::size_t n){++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
void* operator new(std::size_t n,std::align_val_t a){++allocations;if(auto* p=_aligned_malloc(n?n:1,static_cast<std::size_t>(a)))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete(void* p,std::align_val_t) noexcept{_aligned_free(p);}
void operator delete[](void* p,std::align_val_t) noexcept{_aligned_free(p);}
void operator delete(void* p,std::size_t,std::align_val_t) noexcept{_aligned_free(p);}
void operator delete[](void* p,std::size_t,std::align_val_t) noexcept{_aligned_free(p);}
using namespace entropy::standalone;
static int failed=0;
static void check(bool ok,const char* name,double e=0){std::printf("%s %s (max error %.3g)\n",ok?"PASS":"FAIL",name,e);if(!ok)++failed;}
static double diff(const std::vector<double>& a,const std::vector<double>& b){double e=0;for(std::size_t i=0;i<a.size();++i){if(!std::isfinite(a[i]))return INFINITY;e=std::max(e,std::abs(a[i]-b[i]));}return e;}
// Independent scalar time-domain oracle: no production FFT/kernel helpers.
static std::vector<double> convolution(const std::vector<double>& x,const std::vector<double>& h){std::vector<double> y(x.size());for(std::size_t i=0;i<x.size();++i){long double s=0;for(std::size_t j=0;j<h.size()&&j<=i;++j)s+=static_cast<long double>(h[j])*x[i-j];y[i]=static_cast<double>(s);}return y;}
static std::vector<double> taps(int n,double phase=0){std::vector<double> h(n);for(int i=0;i<n;++i)h[i]=std::sin(i*0.173+phase)*0.03/(1+i*0.003);h[0]=0.7;h[127]=-0.11;h[128]=0.23;h[n-1]=0.19;return h;}
static std::vector<double> input(int n){std::vector<double> x(n);unsigned int s=12345;for(auto& v:x){s=1664525u*s+1013904223u;v=(static_cast<double>(s)/4294967296.0-0.5)*4;}return x;}
static void fragmented(RealtimeFir& fir,std::vector<double>& x,const FirKernel* k){const int frames[]={0,1,7,127,128,129,255,511,3,64};int at=0,b=0;while(at<static_cast<int>(x.size())){int n=std::min(frames[b++%10],static_cast<int>(x.size())-at);fir.process(x.data()+at,n,k,nullptr,1,0);at+=n;}}
static void fftTests(){FixedFFT fft;std::array<FixedFFT::C,256>a{};a[0]=1;fft.run(a.data());double e=0;for(const auto& z:a)e=std::max(e,std::abs(z-FixedFFT::C(1,0)));check(e<1e-13,"FFT impulse unscaled",e);for(int i=0;i<256;++i)a[i]={std::cos(i*0.31),std::sin(i*0.47)};const auto original=a;fft.run(a.data());fft.run(a.data(),true);e=0;for(int i=0;i<256;++i)e=std::max(e,std::abs(a[i]-original[i]));check(e<1e-12,"FFT complex roundtrip inverse normalization",e);for(int i=0;i<256;++i)a[i]=std::polar(1.0,6.283185307179586476925286766559*17*i/256.0);fft.run(a.data());e=0;for(int i=0;i<256;++i)e=std::max(e,std::abs(a[i]-FixedFFT::C(i==17?256.0:0,0)));check(e<2e-11,"FFT positive complex tone bin/sign",e);}
static void firTests(){FixedFFT fft;for(int n:{2049,4097,8193}){auto h=taps(n);auto k=std::make_unique<FirKernel>();k->build(h.data(),n,fft);auto fir=std::make_unique<RealtimeFir>();fir->reset();auto x=input(n+900),expected=convolution(x,h),actual=x;fragmented(*fir,actual,k.get());double e=diff(actual,expected);char name[128];std::snprintf(name,sizeof(name),"FIR %d direct convolution variable frames",n);check(e<1e-11,name,e);fir->reset();std::vector<double> impulse(n+300);impulse[0]=1;expected.assign(impulse.size(),0);std::copy(h.begin(),h.end(),expected.begin());fragmented(*fir,impulse,k.get());e=diff(impulse,expected);std::snprintf(name,sizeof(name),"FIR %d impulse no added latency",n);check(e<1e-12,name,e);const auto before=allocations.load();fir->reset();double silence[513]{};fir->process(silence,0,k.get(),nullptr,1,0);fir->process(silence,513,k.get(),nullptr,1,0);check(allocations.load()==before,"FIR reset/process no C++ allocation");}}
static void transitionTest(){FixedFFT fft;auto oldH=taps(2049,0.1),newH=taps(8193,0.7);auto oldK=std::make_unique<FirKernel>(),newK=std::make_unique<FirKernel>();oldK->build(oldH.data(),static_cast<int>(oldH.size()),fft);newK->build(newH.data(),static_cast<int>(newH.size()),fft);auto x=input(10400),yOld=convolution(x,oldH),yNew=convolution(x,newH),expected=yOld,actual=x;const int adoption=8999,fade=313;for(int i=adoption;i<static_cast<int>(x.size());++i){double t=std::min(1.0,(i-adoption)/double(fade));expected[i]=yOld[i]+(yNew[i]-yOld[i])*t;}auto fir=std::make_unique<RealtimeFir>();fir->reset();fir->process(actual.data(),adoption,oldK.get(),nullptr,1,0);const int frames[]={0,1,7,128,255,3};int at=adoption,b=0;while(at<static_cast<int>(x.size())){int n=std::min(frames[b++%6],static_cast<int>(x.size())-at);fir->process(actual.data()+at,n,newK.get(),oldK.get(),(at-adoption)/double(fade),1.0/fade);at+=n;}double e=diff(actual,expected);check(e<2e-11,"FIR warm long-kernel adoption midpartition/crossfade",e);std::vector<double> cold(1024);fir->reset();fragmented(*fir,cold,newK.get());check(diff(cold,std::vector<double>(cold.size()))==0,"FIR reset clears warm history");}
static void supportTest(){FixedFFT fft;auto k=std::make_unique<FirKernel>();std::vector<double> h(8193);h[8192]=0.83;k->build(h.data(),8193,fft);check(k->firstNonzeroTap==8192,"FIR causal support marker matches delayed impulse");auto fir=std::make_unique<RealtimeFir>();fir->reset();std::vector<double>x(9000);x[0]=1;auto expected=convolution(x,h);fragmented(*fir,x,k.get());check(diff(x,expected)<1e-13,"FIR final partial partition tap exactly at 8192",diff(x,expected));double noKernel[17];std::fill(std::begin(noKernel),std::end(noKernel),1.0);fir->reset();fir->process(noKernel,17,nullptr,nullptr,1,0);double e=0;for(double v:noKernel)e=std::max(e,std::abs(v));check(e==0,"FIR null kernel produces silence");}
static void spectrumDoubleTest(){constexpr int n=16384;entropy::native::IppSpectrum spectrum(14);std::vector<double>x(n),y(n/2+1);for(int i=0;i<n;++i)x[i]=0.25+std::cos(6.283185307179586476925286766559*37*i/n);spectrum.magnitudes(x.data(),y.data());double e=0;for(int i=0;i<=n/2;++i)e=std::max(e,std::abs(y[i]-(i==0?n*0.25:(i==37?n/2.0:0))));check(e<1e-9,"Spectrum FP64 input preserves DC and tone amplitude",e);}
static void spectrumTests(){constexpr int n=16384;entropy::native::IppSpectrum spectrum(14);std::vector<float>x(n);std::vector<double>y(n/2+1);x[0]=1;spectrum.magnitudes(x.data(),y.data());double e=0;for(double v:y)e=std::max(e,std::abs(v-1));check(e<1e-13,"Spectrum order14 impulse raw FP64 magnitudes",e);for(int i=0;i<n;++i)x[i]=static_cast<float>(std::sin(6.283185307179586476925286766559*53*i/n));spectrum.magnitudes(x.data(),y.data());e=0;for(int i=0;i<=n/2;++i)e=std::max(e,std::abs(y[i]-(i==53?n/2.0:0)));check(e<1e-4,"Spectrum float monitor converted to FP64 tone bin",e);const auto before=allocations.load();spectrum.magnitudes(x.data(),y.data());check(allocations.load()==before,"Spectrum prepared workspace has no C++ allocation");}
int main(){fftTests();firTests();transitionTest();supportTest();spectrumTests();spectrumDoubleTest();std::printf("RESULT %d failure(s)\n",failed);return failed?1:0;}


