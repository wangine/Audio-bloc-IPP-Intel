#include "SourceFifo192.h"
#include "reference/BaselineSourceFifo192.h"
#include <array>
#include <bit>
#include <chrono>
#include <cstring>
#include <iostream>
#include <vector>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<" "<<#x<<"\n";std::exit(1);}}while(0)
using namespace entropy::host;
static uint64_t seed=0x901284751234ULL;
static uint64_t randomBits(){seed^=seed<<13;seed^=seed>>7;return seed^=seed<<17;}
template<class F> static double bench(){F f;CHECK(f.prepare(8,8192));CHECK(f.reset(1));std::array<std::vector<double>,8>b;std::array<const double*,8>in;std::array<double*,8>out;for(unsigned c=0;c<8;++c){b[c].resize(2048);in[c]=b[c].data();out[c]=b[c].data();}auto begin=std::chrono::steady_clock::now();for(unsigned i=0;i<20000;++i){auto n=std::array<unsigned,3>{256,1024,2048}[i%3];uint64_t g;CHECK(f.push(in.data(),n,1));CHECK(f.pop(out.data(),n,g));}return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();}
int main(){for(unsigned ch:{2u,8u})for(unsigned cap:{4096u,4103u}){
 SourceFifo192 f;entropy::fifo_reference::SourceFifo192 ref;CHECK(f.prepare(ch,cap)&&ref.prepare(ch,cap));
 SourceStreamDescriptor d{1,192000,ch,64,64,1};entropy::fifo_reference::SourceStreamDescriptor rd{1,192000,ch,64,64,1};CHECK(f.reset(d)&&ref.reset(rd));
 std::array<std::vector<double>,8> in,a,b;std::array<const double*,8> ip{};std::array<double*,8> ap{},bp{};
 for(unsigned c=0;c<ch;++c){in[c].resize(2048);a[c].resize(2048);b[c].resize(2048);ip[c]=in[c].data();ap[c]=a[c].data();bp[c]=b[c].data();}
 for(unsigned step=0;step<12000;++step){auto choice=randomBits();auto n=std::array<unsigned,7>{1,7,255,256,1024,2048,0}[choice%7];
  if(step%257==0){++d.generation;++rd.generation;CHECK(f.reset(d)==ref.reset(rd));}
  if(choice&8){for(unsigned c=0;c<ch;++c)for(unsigned i=0;i<n;++i)in[c][i]=std::bit_cast<double>(randomBits());
   if(choice&16){auto wrong=d;auto wr=rd;++wrong.format_revision;++wr.format_revision;CHECK(f.push(ip.data(),n,wrong)==ref.push(ip.data(),n,wr));}
   else CHECK(f.push(ip.data(),n,d)==ref.push(ip.data(),n,rd));
  }else{SourceStreamDescriptor got;entropy::fifo_reference::SourceStreamDescriptor rg;const bool ok=f.pop(ap.data(),n,got),rk=ref.pop(bp.data(),n,rg);CHECK(ok==rk);if(ok){CHECK(got.generation==rg.generation&&got.format_revision==rg.format_revision);for(unsigned c=0;c<ch;++c)CHECK(std::memcmp(ap[c],bp[c],n*sizeof(double))==0);}}
  auto x=f.telemetry();auto y=ref.telemetry();CHECK(x.fill_frames==y.fill_frames&&x.produced_frames==y.produced_frames&&x.consumed_frames==y.consumed_frames&&x.underruns==y.underruns&&x.overflows==y.overflows&&x.abandoned_frames==y.abandoned_frames&&x.high_water_frames==y.high_water_frames&&x.low_water_frames==y.low_water_frames);
 }
}
std::cout<<"PASS FP64 bit copies, descriptors, counters: 2/8ch, power/nonpower capacity, reset/wrap/full/empty; special FP64 bit patterns\n";
std::cout<<"reference_ms="<<bench<entropy::fifo_reference::SourceFifo192>()<<" candidate_ms="<<bench<SourceFifo192>()<<"\n";
}
