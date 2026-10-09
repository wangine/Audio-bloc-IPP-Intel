#include "MultiChannelSrcEngine.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>
#define CHECK(x) do{if(!(x)){std::printf("FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
 using entropy::src::MultiChannelSrcEngine;
 EntropySrcConfig c{};c.struct_size=sizeof(c);c.engine=ENTROPY_SRC_R8BRAIN_REFERENCE;
 c.source_rate=192000;c.destination_rate=96000;c.channels=2;c.maximum_input_frames=256;
 MultiChannelSrcEngine e;
 CHECK(!e.configure(c)); // Input SRC policy stays ->192 kHz.
 CHECK(e.configureNominalEgress(c));
 c.engine=ENTROPY_SRC_SINC_LAB;CHECK(!e.configureNominalEgress(c));CHECK(!e.configure(c));
 c.engine=ENTROPY_SRC_R8BRAIN_REFERENCE;c.destination_rate=48000;CHECK(!e.configureNominalEgress(c));
 c.destination_rate=192000;CHECK(e.configureNominalEgress(c));
 std::array<double,256> left{},right{},outLeft{},outRight{};
 left[0]=-0.0;left[1]=2.5;left[2]=std::numeric_limits<double>::denorm_min();
 right[0]=-3.25;right[1]=.125;right[2]=-std::numeric_limits<double>::denorm_min();
 const double* in[]={left.data(),right.data()};double* out[]={outLeft.data(),outRight.data()};
 EntropySrcBlockF64 b{};b.struct_size=sizeof(b);b.channels=2;b.inputs=in;b.outputs=out;b.input_frames=256;b.output_capacity=256;
 CHECK(e.process(b)==ENTROPY_OK&&b.consumed_frames==256&&b.produced_frames==256);
 CHECK(!std::memcmp(left.data(),outLeft.data(),sizeof(left))&&!std::memcmp(right.data(),outRight.data(),sizeof(right)));
 c.destination_rate=96000;CHECK(e.configureNominalEgress(c));
 std::vector<double> a(e.config().required_output_capacity),d(a.size());out[0]=a.data();out[1]=d.data();b.output_capacity=static_cast<unsigned>(a.size());
 std::vector<double> first,second;
 for(auto* result:{&first,&second}){
  e.reset();
  for(unsigned block=0;block<160;++block){
   for(unsigned f=0;f<256;++f){left[f]=.9*std::sin(.03*(block*256+f));right[f]=-left[f];}
   CHECK(e.process(b)==ENTROPY_OK&&b.consumed_frames==256);
   for(unsigned f=0;f<b.produced_frames;++f)CHECK(std::abs(a[f]+d[f])<1e-14);
   result->insert(result->end(),a.begin(),a.begin()+b.produced_frames);
  }
 }
 CHECK(!first.empty()&&first.size()<=20480&&first.size()==second.size());
 CHECK(!std::memcmp(first.data(),second.data(),first.size()*sizeof(double)));
 unsigned steadyFrames=0;
 for(unsigned block=0;block<160;++block){CHECK(e.process(b)==ENTROPY_OK);steadyFrames+=b.produced_frames;}
 CHECK(steadyFrames==20480); // 40960 additional input frames at exactly 2:1.
 std::puts("PASS input policy / LAB rejection / nominal wire bit-copy / 192->96 / exact reset replay");
}
