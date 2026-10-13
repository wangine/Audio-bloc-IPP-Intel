#include "TestSupport.h"
#include "DspStageRuntime.h"
#include "DspEqModule.h"
#include <cmath>
int main(){
 auto s=session(1);StageBusWin32 source,consumer;DspStageRuntime dsp;CenterSettings center;center.values[0]=0;std::string error;
 auto eq=defaultEqSettings();eq.mode=1;eq.bands[0].frequency_hz=1000;eq.bands[0].static_gain_db=6;
 DspEqModule reference;CHECK(reference.prepare(CENTER_MODULE_ROOT,error));CHECK(reference.apply(eq,1));
 CHECK(dsp.prepareCenter(CENTER_MODULE_ROOT,center,error));
 CHECK(dsp.prepareEq(CENTER_MODULE_ROOT,eq,error));
 CHECK(source.create(s,error));CHECK(dsp.attach(s,error));CHECK(consumer.open(dsp.outputSession(),error));
 Planes<10> in;Planes<11> out;std::array<double,1024> refL{},refR{};double dryEnergy=0,wetEnergy=0;
 for(uint64_t sequence=1;sequence<=48;sequence++){
  for(unsigned i=0;i<1024;i++){double t=(sequence-1)*1024+i;in.data[0][i]=.1*std::sin(t*2*3.141592653589793*1000/192000);in.data[1][i]=.08*std::sin(t*2*3.141592653589793*1000/192000+.3);for(unsigned c=2;c<10;c++){uint64_t payload=i%3==0?0x8000000000000000ULL:i%3==1?0x7ff8000000000000ULL+i:0x3ff0000000000000ULL+c;std::memcpy(&in.data[c][i],&payload,8);}}
  CHECK(reference.process(in.in[0],in.in[1],refL.data(),refR.data()));
  CHECK(consumer.trySendRequest(request(s,sequence))==StageBusIoResult::Ok);CHECK(dsp.step(qpc())!=DspStepResult::Fault);StageBusFrameRequest upstream{};CHECK(source.tryReceiveRequest(upstream)==StageBusIoResult::Ok);auto b=header(upstream);CHECK(source.tryPublish(b,in.in.data())==StageBusIoResult::Ok);CHECK(dsp.step(qpc())==DspStepResult::Published);StageBusBlockHeader h{};CHECK(consumer.tryConsume(h,out.out.data())==StageBusIoResult::Ok);CHECK(!std::memcmp(&b,&h,sizeof(h)));
  CHECK(!std::memcmp(refL.data(),out.in[0],8192));CHECK(!std::memcmp(refR.data(),out.in[1],8192));
  for(unsigned i=0;i<1024;i++){CHECK(out.data[2][i]==.5*(in.data[0][i]+in.data[1][i]));if(sequence>24){dryEnergy+=in.data[0][i]*in.data[0][i];wetEnergy+=out.data[0][i]*out.data[0][i];}}
  for(unsigned c=2;c<10;c++)CHECK(!std::memcmp(in.in[c],out.in[c+1],8192));
 }
 CHECK(std::sqrt(wetEnergy/dryEnergy)>1.9);
 std::cout<<"PASS EQ slot 2 LR exact native output; Center raw tap unchanged; all Cinema payloads and timeline unchanged\n";
}
