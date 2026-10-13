#include "TestSupport.h"
#include "DspStageRuntime.h"
#include <future>
#include <cmath>
#ifdef PASSIVE_ALLOCATION_AUDIT
#include "RealtimeAllocationAudit.h"
#endif

// Real passive queues exercise the unchanged EQ -> raw-tap Center -> LRC
// order. Sticky provider flags must not erase native history on every block.
int main(){
 const auto sourceEndpoint=entropy::publish::endpointName()+L".native-source."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(qpc());
 const auto outputEndpoint=sourceEndpoint+L".output";
 std::string error;auto s=session(MIX_CINEMA10_V1);s.producer_pid=GetCurrentProcessId();
 entropy::publish::Publisher source,output;CHECK(source.start(sourceEndpoint,error));source.setSession(s);CHECK(output.start(outputEndpoint,error));
 DspStageRuntime runtime;runtime.setPublisher(&output);
 auto eq=defaultEqSettings();eq.mode=1;eq.bands[0].frequency_hz=1000;eq.bands[0].static_gain_db=6;
 CenterSettings center;center.values[0]=0;
 CHECK(runtime.prepareEq(CENTER_MODULE_ROOT,eq,error));CHECK(runtime.prepareCenter(CENTER_MODULE_ROOT,center,error));CHECK(runtime.prepareAligner(CENTER_MODULE_ROOT,error));
 DspEqModule reference;CHECK(reference.prepare(CENTER_MODULE_ROOT,error));CHECK(reference.apply(eq,1));CHECK(reference.reset());
 auto attach=std::async(std::launch::async,[&]{return runtime.attachPassive(sourceEndpoint,error);});
 auto deadline=qpc()+frequency()*3;while(attach.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready && qpc()<deadline){source.service();Sleep(1);}CHECK(attach.get());
 entropy::publish::Subscriber subscriber;
 auto connect=std::async(std::launch::async,[&]{return subscriber.connect(outputEndpoint,error);});
 deadline=qpc()+frequency()*3;while(connect.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready && qpc()<deadline){output.service();Sleep(1);}CHECK(connect.get());
 Planes<10> in;Planes<11> out;std::array<double,1024> refL{},refR{};
 const uint64_t endSequence=
#ifdef PASSIVE_ALLOCATION_AUDIT
  1100;
 CHECK(entropy::host::allocation_audit::installHooks());CHECK(entropy::host::allocation_audit::selfTest());
#else
  119;
#endif
 for(uint64_t sequence=71;sequence<endSequence;sequence++){
  for(unsigned f=0;f<1024;f++){
   const double t=(sequence-71)*1024+f;in.data[0][f]=.1*std::sin(t*2*3.141592653589793*1000/192000);in.data[1][f]=.08*std::sin(t*2*3.141592653589793*1000/192000+.3);
   for(unsigned c=2;c<10;c++){const uint64_t value=f%3==0?0x8000000000000000ULL:f%3==1?0x7ff8000000000000ULL+f:0x3ff0000000000000ULL+c;std::memcpy(&in.data[c][f],&value,8);}
  }
  if(sequence==95)CHECK(reference.reset());
  CHECK(reference.process(in.in[0],in.in[1],refL.data(),refR.data()));
  auto b=header(request(s,sequence));b.control_revision=31;b.discontinuity_mask=1;if(sequence>=95)++b.source_generation[0];
  source.service();StageBusBlockHeader received{};
#ifdef PASSIVE_ALLOCATION_AUDIT
  if(sequence==100)entropy::host::allocation_audit::reset();
  if(sequence>=100){entropy::host::allocation_audit::begin();entropy::host::allocation_audit::audioPhase();}
#endif
  source.publish(b,in.in.data());const auto processed=runtime.step(qpc());const auto read=subscriber.read(received,out.out.data());
#ifdef PASSIVE_ALLOCATION_AUDIT
  if(sequence>=100)entropy::host::allocation_audit::end();
#endif
  CHECK(processed==DspStepResult::Published);CHECK(read==entropy::publish::ReadResult::Block);CHECK(!std::memcmp(&b,&received,sizeof(b)));
  CHECK(!std::memcmp(refL.data(),out.in[0],8192) && !std::memcmp(refR.data(),out.in[1],8192));
  for(unsigned f=0;f<1024;f++)CHECK(out.data[2][f]==.5*(in.data[0][f]+in.data[1][f]));
  for(unsigned c=2;c<10;c++)CHECK(!std::memcmp(in.in[c],out.in[c+1],8192));
 }
 CHECK(runtime.diagnostics().blocksIn==endSequence-71 && runtime.diagnostics().blocksOut==endSequence-71);
#ifdef PASSIVE_ALLOCATION_AUDIT
 const auto counts=entropy::host::allocation_audit::snapshot();entropy::host::allocation_audit::report();
 CHECK(counts.cppNew==0 && counts.cppDelete==0 && counts.heapAlloc==0 && counts.heapFree==0 && counts.wait==0 && counts.lock==0 && counts.io==0);
 std::cout<<"PASS 1000 warmed real passive source publish/runtime DSP/output read cycles with zero audited allocation/free/wait/lock/io; control service, lifecycle and explicit history resets excluded\n";
#endif
 subscriber.close();runtime.stop();runtime.setPublisher(nullptr);
 std::cout<<"PASS real passive native EQ exact reference, independent raw Center tap, LRC bypass, Cinema NaN/signed-zero bits, immutable headers, sticky flags and generation reset\n";
}
