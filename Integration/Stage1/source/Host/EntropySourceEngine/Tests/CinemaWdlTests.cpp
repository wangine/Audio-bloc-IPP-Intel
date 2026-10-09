#include "CinemaWdlSrc.h"
#include <array>
#include <vector>
#include <iostream>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <new>
using namespace entropy::host;
static thread_local bool track=false;
static thread_local unsigned allocations=0, frees=0;
extern "C" void* __real_malloc(size_t); extern "C" void* __real_realloc(void*,size_t); extern "C" void __real_free(void*);
extern "C" void* __wrap_malloc(size_t n){if(track)++allocations;return __real_malloc(n);}
extern "C" void* __wrap_realloc(void*p,size_t n){if(track)++allocations;return __real_realloc(p,n);}
extern "C" void __wrap_free(void*p){if(track&&p)++frees;__real_free(p);}
void* operator new(size_t n){if(track)++allocations;auto*p=__real_malloc(n?n:1);if(!p)throw std::bad_alloc();return p;}
void operator delete(void*p) noexcept {if(track&&p)++frees;__real_free(p);}
void operator delete(void*p,size_t) noexcept {::operator delete(p);}
#define CHECK(x) do {if(!(x)){track=false;std::cerr<<"FAIL line "<<__LINE__<<" "<<#x<<'\n';return 1;}}while(0)
static double sample(unsigned n,unsigned c,unsigned rate){return (c+1)*0.02*std::sin(6.283185307179586*1000*n/rate)+(n==4096?0.25:0);}
int main(){for(unsigned taps:{64u,128u}){
 CinemaWdlSrc src;CHECK(src.prepare(256,taps));
 for(unsigned rate:{48000u,96000u,192000u,44100u,88200u,176400u,8000u,768000u,48001u}){
  const unsigned total=16384;std::array<std::array<double,256>,8>in{};std::array<const double*,8>ip{};std::array<std::vector<double>,8>out,stream;std::array<double*,8>op{};
  for(unsigned c=0;c<8;++c){ip[c]=in[c].data();out[c].resize(src.outputCapacity());op[c]=out[c].data();stream[c].reserve(size_t(total)*24);}
  allocations=frees=0;track=true;bool configured=src.configure(rate);track=false;CHECK(configured);CHECK(!allocations&&!frees);
  unsigned first=0;double latency=0;
  while(first<total){unsigned frames=std::min(256u,total-first);for(unsigned c=0;c<8;++c)for(unsigned i=0;i<frames;++i)in[c][i]=sample(first+i,c,rate);unsigned n=0;
   track=true;bool ok=src.process(ip.data(),frames,op.data(),src.outputCapacity(),n);latency=src.latencySeconds();track=false;CHECK(ok);CHECK(!allocations&&!frees);
   for(unsigned c=0;c<8;++c)stream[c].insert(stream[c].end(),out[c].begin(),out[c].begin()+n);first+=frames;
  }
  if(rate==192000){CHECK(src.bypass());CHECK(latency==0);CHECK(stream[0].size()==total);for(unsigned c=0;c<8;++c)for(unsigned i=0;i<total;++i)CHECK(stream[c][i]==sample(i,c,rate));}
  else {
   // Independent upstream WDL call with one complete input block: validates
   // segmentation, no duplicate/lost output samples and common channel phase.
   WDL_Resampler ref;ref.SetMode(false,0,true,taps,32);ref.SetFeedMode(true);ref.SetRates(rate,192000);double* input=nullptr;CHECK(ref.ResamplePrepare(total,8,&input)==int(total));for(unsigned i=0;i<total;++i)for(unsigned c=0;c<8;++c)input[i*8+c]=sample(i,c,rate);std::vector<double>expected(size_t(total)*24*8);int n=ref.ResampleOut(expected.data(),total,int(expected.size()/8),8);CHECK(n>=0);CHECK(stream[0].size()==size_t(n));double maxError=0;for(unsigned c=0;c<8;++c)for(int i=0;i<n;++i)maxError=std::max(maxError,std::abs(stream[c][i]-expected[size_t(i)*8+c]));std::cout<<"SEGMENTATION rate="<<rate<<" max_error="<<maxError<<'\n';CHECK(maxError<1e-9);
   CHECK(latency*192000<=(taps+4)*192000.0/rate);
   if(rate==48000||rate==96000){double err=0;for(unsigned i=2048;i<stream[0].size()-1024;++i)if(std::abs(double(i)-4096.0*192000/rate)>taps*192000.0/rate)err=std::max(err,std::abs(stream[0][i]-0.02*std::sin(6.283185307179586*1000*i/192000)));CHECK(err<0.00001);
    double imageSin=0,imageCos=0;
    for(unsigned i=1024;i<7168;++i){const double phase=6.283185307179586*(rate-1000)*i/192000;imageSin+=stream[0][i]*std::sin(phase);imageCos+=stream[0][i]*std::cos(phase);}
    const double imageDb=20*std::log10(2*std::hypot(imageSin,imageCos)/6144/0.02);
    std::cout<<"QUALITY rate="<<rate<<" taps="<<taps<<" passband_1khz_max_error="<<err<<" image_db="<<imageDb<<'\n';
    CHECK(imageDb < -90);
}
  }
  std::cout<<"PASS taps="<<taps<<" rate="<<rate<<" output="<<stream[0].size()<<" latency_output_samples="<<latency*192000<<" latency_ms="<<latency*1000<<" allocations="<<allocations<<" frees="<<frees<<'\n';
 }
}
}
