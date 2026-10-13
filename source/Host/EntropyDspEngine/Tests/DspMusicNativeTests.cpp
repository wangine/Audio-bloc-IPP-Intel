#include "MusicTestSupport.h"
#include "DspStageRuntime.h"
#include <cmath>
#ifdef MUSIC_ALLOCATION_AUDIT
#include "RealtimeAllocationAudit.h"
#endif
int main(){
 using namespace entropy::publish;std::string error;auto endpoint=musicEndpoint(L".native");
 auto s=session(3);s.producer_pid=GetCurrentProcessId();Publisher source,output;
 CHECK(source.start(endpoint,error));source.setSession(s);CHECK(output.start(endpoint+L".out",error));
 auto eq=defaultEqSettings();eq.mode=1;eq.bands[0].frequency_hz=1000;eq.bands[0].static_gain_db=6;
 CenterSettings center;center.values[0]=0;
 DspStageRuntime runtime;runtime.setPublisher(&output);
 CHECK(runtime.prepareEq(CENTER_MODULE_ROOT,eq,error));CHECK(runtime.prepareCenter(CENTER_MODULE_ROOT,center,error));CHECK(runtime.prepareAligner(CENTER_MODULE_ROOT,error));
 DspEqModule refEq;DspCenterModule refCenter;DspLrcModule refAlign;
 CHECK(refEq.prepare(CENTER_MODULE_ROOT,error));CHECK(refEq.apply(eq,1));CHECK(refEq.reset());
 CHECK(refCenter.prepare(CENTER_MODULE_ROOT,error));CHECK(refCenter.apply(center,1));CHECK(refCenter.reset());
 CHECK(refAlign.prepare(CENTER_MODULE_ROOT,error));CHECK(refAlign.reset());
 CHECK(musicConnect(source,[&]{return runtime.attachPassive(endpoint,error);}));
 Subscriber subscriber;CHECK(musicConnect(output,[&]{return subscriber.connect(endpoint+L".out",error);}));
 Planes<2> in;Planes<3> out,reference,aligned;
#ifdef MUSIC_ALLOCATION_AUDIT
 constexpr uint64_t endSequence=1200;CHECK(entropy::host::allocation_audit::installHooks());CHECK(entropy::host::allocation_audit::selfTest());
#else
 constexpr uint64_t endSequence=120;
#endif
 for(uint64_t seq=71;seq<endSequence;seq++){
  if(seq==85){center.values[0]=1;center.values[1]=0;CHECK(runtime.applyCenter(center,2));CHECK(refCenter.apply(center,2));}
  if(seq==95){CHECK(refEq.reset());CHECK(refCenter.reset());CHECK(refAlign.reset());}
  if(seq==100){auto a=runtime.diagnostics().alignment;CHECK(a.valid);CHECK(runtime.applyAlignment(true,runtime.diagnostics().center.revision,a.detected[2])==AlignmentApplyResult::Applied);a=runtime.diagnostics().alignment;CHECK(refAlign.configure(true,a.current,a.appliedTarget));CHECK(a.current[0]>0 || a.current[1]>0 || a.current[2]>0);}
  if(seq==110){CHECK(runtime.applyAlignment(false,0,0)==AlignmentApplyResult::Applied);CHECK(refAlign.configure(false,{},0));}
  for(unsigned f=0;f<1024;f++){auto t=(seq-71)*1024+f;in.data[0][f]=.1*std::sin(t*2*3.141592653589793*1000/192000);in.data[1][f]=.08*std::sin(t*2*3.141592653589793*1000/192000+.3);}
  CHECK(refEq.process(in.in[0],in.in[1],reference.out[0],reference.out[1]));
  CHECK(refCenter.process(in.in[0],in.in[1],reference.out[2]));CHECK(refAlign.process(reference.in.data(),aligned.out.data()));
  auto b=musicHeader(s,seq);b.discontinuity_mask=1;if(seq>=95)++b.source_generation[0];source.service();
#ifdef MUSIC_ALLOCATION_AUDIT
  if(seq==200)entropy::host::allocation_audit::reset();
  if(seq>=200){entropy::host::allocation_audit::begin();entropy::host::allocation_audit::audioPhase();}
#endif
  source.publish(b,in.in.data());auto processed=runtime.step(qpc());StageBusBlockHeader received{};auto read=subscriber.read(received,out.out.data());
#ifdef MUSIC_ALLOCATION_AUDIT
  if(seq>=200)entropy::host::allocation_audit::end();
#endif
  CHECK(processed==DspStepResult::Published && read==ReadResult::Block);CHECK(!std::memcmp(&b,&received,sizeof(b)));
  for(unsigned c=0;c<3;c++)CHECK(!std::memcmp(out.in[c],aligned.in[c],8192));
 }
#ifdef MUSIC_ALLOCATION_AUDIT
 auto counts=entropy::host::allocation_audit::snapshot();entropy::host::allocation_audit::report();
 CHECK(counts.cppNew==0 && counts.cppDelete==0 && counts.heapAlloc==0 && counts.heapFree==0 && counts.wait==0 && counts.lock==0 && counts.io==0);
#endif
 subscriber.close();runtime.stop();runtime.setPublisher(nullptr);
 std::cout<<"PASS MUSIC native EQ/raw Center/LRC exact DLL references, active alignment and bypass, generation reset/sticky flags\n";
}
