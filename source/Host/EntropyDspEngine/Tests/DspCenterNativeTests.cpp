#include "TestSupport.h"
#include "DspCenterModule.h"
#include <cmath>
#include <limits>
int main(){DspCenterModule center;std::string error;CHECK(center.prepare(CENTER_MODULE_ROOT,error));CHECK(center.set(0,0));Planes<2> in;std::array<double,1024> out{};for(unsigned i=0;i<1024;i++){in.data[0][i]=1.5;in.data[1][i]=1.5;}CHECK(center.process(in.data[0].data(),in.data[1].data(),out.data()));for(auto x:out)CHECK(x==1.5);CHECK(center.snapshot().latencyValid);auto changed=center.snapshot().settings;changed.values[2]=.6;CHECK(center.apply(changed,99));CHECK(center.snapshot().latencyValid);CHECK(center.snapshot().revision==99);CHECK(center.set(15,0));CHECK(center.process(in.data[0].data(),in.data[1].data(),out.data()));for(auto x:out)CHECK(bits(x)==0);CHECK(center.set(15,1));for(unsigned i=0;i<1024;i++)in.data[1][i]=-1.5;CHECK(center.process(in.data[0].data(),in.data[1].data(),out.data()));for(auto x:out)CHECK(bits(x)==0);
// SUM keeps precision beyond FP32; lookahead is a real delay on C only.
CHECK(center.set(0,0));CHECK(center.set(13,1.25));CHECK(center.reset());
for(unsigned block=0;block<3;block++){
 for(unsigned i=0;i<1024;i++)in.data[0][i]=in.data[1][i]=(block==0&&i==0)?1.0000000000000002:0.0;
 CHECK(center.process(in.data[0].data(),in.data[1].data(),out.data()));
 CHECK(center.snapshot().latencyValid && center.snapshot().telemetry.latency_samples==240);
 for(unsigned i=0;i<1024;i++)CHECK(bits(out[i])==bits((block==0&&i==240)?1.0000000000000002:0.0));
}
for(unsigned fft=0;fft<5;fft++){
 CHECK(center.set(0,1));CHECK(center.set(1,fft));CHECK(center.set(13,0));CHECK(center.reset());
 CHECK(!center.snapshot().latencyValid);
 double total=0;
 for(unsigned block=0;block<24;block++){
  for(unsigned i=0;i<1024;i++)in.data[0][i]=in.data[1][i]=.5*std::sin((block*1024+i)*.03);
  CHECK(center.process(in.data[0].data(),in.data[1].data(),out.data()));
  CHECK(center.snapshot().telemetry.latency_samples==(512u<<fft));
  for(auto x:out){CHECK(std::isfinite(x));total+=std::abs(x);}
 }
 CHECK(total>1);std::cout<<"FFT "<<(512u<<fft)<<": reported C latency "<<center.snapshot().telemetry.latency_samples<<" samples\n";
}
in.data[0][0]=std::numeric_limits<double>::infinity();CHECK(!center.process(in.data[0].data(),in.data[1].data(),out.data()));
DspCenterModule missing;CHECK(!missing.prepare(std::filesystem::temp_directory_path()/"entropy-missing-center",error));
std::cout<<"PASS native V2 LR->C, SUM unclipped above unity, enable off +zero, antiphase cancels\n";}
