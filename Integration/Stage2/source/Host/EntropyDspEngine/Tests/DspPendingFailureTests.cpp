#include "TestSupport.h"
#include "DspStageRuntime.h"
int main(){for(unsigned scenario=0;scenario<4;scenario++){
 auto s=session(1);StageBusWin32 source,render;DspStageRuntime dsp;std::string error;Planes<10> audio;CHECK(source.create(s,error));CHECK(dsp.attach(s,error));CHECK(render.open(dsp.outputSession(),error));
 for(uint64_t seq=1;seq<=5;seq++){auto r=request(s,seq);CHECK(render.trySendRequest(r)==StageBusIoResult::Ok);CHECK(dsp.step(qpc())!=DspStepResult::Fault);StageBusFrameRequest received{};CHECK(source.tryReceiveRequest(received)==StageBusIoResult::Ok);auto b=header(received);CHECK(source.tryPublish(b,audio.in.data())==StageBusIoResult::Ok);CHECK(dsp.step(qpc())!=DspStepResult::Fault);}
 CHECK(dsp.pending() && dsp.diagnostics().blocksIn==5 && dsp.diagnostics().blocksOut==4);
 if(scenario==0){dsp.stop();CHECK(source.trySendRequest(request(s,6))==StageBusIoResult::Fault);CHECK(render.trySendRequest(request(s,6))==StageBusIoResult::Fault);}
 else if(scenario==1){source.fault();CHECK(!dsp.peersAlive(qpc()));CHECK(dsp.faulted());}
 else if(scenario==2){render.fault();CHECK(!dsp.peersAlive(qpc()));CHECK(dsp.faulted());}
 else {CHECK(dsp.step(qpc()+frequency()*2)==DspStepResult::Fault);CHECK(dsp.diagnostics().deadlines==1);}
 dsp.stop();
 }std::cout<<"PASS STOP, Source fault, downstream fault and deadline while output is pending; no overwrite/reuse\n";}
