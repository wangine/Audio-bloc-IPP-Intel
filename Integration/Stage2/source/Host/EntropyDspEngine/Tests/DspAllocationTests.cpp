#include "TestSupport.h"
#include "DspStageRuntime.h"
#include "DspLocalDemand.h"
#include "RealtimeAllocationAudit.h"
#include <future>
using namespace entropy::host;
int main(){auto s=session(1);StageBusWin32 source,render;entropy::publish::Publisher publisher;DspStageRuntime dsp;std::string error;Planes<10> input;Planes<11> output;CHECK(source.create(s,error));CHECK(dsp.attach(s,error));CHECK(render.open(dsp.outputSession(),error));
#ifdef CENTER_MODULE_ROOT
CHECK(dsp.prepareCenter(CENTER_MODULE_ROOT,CenterSettings{},error));
#ifdef EQ_ALLOCATION_TEST
auto eqSettings=defaultEqSettings();eqSettings.mode=3;CHECK(dsp.prepareEq(CENTER_MODULE_ROOT,eqSettings,error));
#endif
#ifdef LRC_ALLOCATION_TEST
CHECK(dsp.prepareAligner(CENTER_MODULE_ROOT,error));
#endif
#endif
// Establish a real V2 subscription outside the audited producer cycles.
CHECK(publisher.start(entropy::publish::endpointName()+L".allocation."+std::to_wstring(GetCurrentProcessId()),error));
publisher.setSession(dsp.outputSession());dsp.setPublisher(&publisher);
entropy::publish::Subscriber passive;auto subscription=std::async(std::launch::async,[&]{return passive.connect(entropy::publish::endpointName()+L".allocation."+std::to_wstring(GetCurrentProcessId()),error);});
auto connectDeadline=qpc()+frequency()*2;while(subscription.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready && qpc()<connectDeadline){publisher.service();Sleep(1);}CHECK(subscription.get());
CHECK(allocation_audit::installHooks());auto cycle=[&](uint64_t seq){auto r=request(s,seq);if(render.trySendRequest(r)!=StageBusIoResult::Ok)return false;if(dsp.step(qpc())==DspStepResult::Fault)return false;StageBusFrameRequest adopted{};if(source.tryReceiveRequest(adopted)!=StageBusIoResult::Ok)return false;auto b=header(adopted);if(source.tryPublish(b,input.in.data())!=StageBusIoResult::Ok)return false;if(dsp.step(qpc())!=DspStepResult::Published)return false;return render.tryConsume(b,output.out.data())==StageBusIoResult::Ok;};for(uint64_t seq=1;seq<=100;seq++){CHECK(cycle(seq));
#ifdef EQ_ALLOCATION_TEST
CHECK(dsp.pollEqTelemetry());
#endif
}
#ifdef LRC_ALLOCATION_TEST
CHECK(dsp.applyAlignment(true,dsp.diagnostics().center.revision,dsp.diagnostics().alignment.detected[2])==AlignmentApplyResult::Applied);
#endif
CHECK(allocation_audit::selfTest());allocation_audit::reset();bool ok=true;for(uint64_t seq=101;seq<=1100;seq++){
#ifdef EQ_ALLOCATION_TEST
eqSettings.bands[0].static_gain_db=double(seq%12)-6;ok &=dsp.applyEq(eqSettings,seq);
#endif
{allocation_audit::CycleScope scope;allocation_audit::audioPhase();
#ifdef CENTER_MODULE_ROOT
CenterSettings changed;changed.values[1]=(seq/100)%5;ok &=dsp.applyCenter(changed,seq);
#endif
ok &=cycle(seq);}
#ifdef EQ_ALLOCATION_TEST
ok &=dsp.pollEqTelemetry();
#endif
}auto counts=allocation_audit::snapshot();DspDiagnostics summary=dsp.diagnostics();dsp.summarizeLatency(summary);CHECK(summary.latencyPercentilesUs[0][0]<=summary.latencyPercentilesUs[0][1] && summary.latencyPercentilesUs[0][1]<=summary.latencyPercentilesUs[0][2]);allocation_audit::report();CHECK(ok);CHECK(counts.cppNew==0 && counts.cppDelete==0 && counts.heapAlloc==0 && counts.heapFree==0 && counts.wait==0 && counts.lock==0 && counts.io==0);CHECK(publisher.dropped()>0);StageBusBlockHeader gap{};CHECK(passive.read(gap,output.out.data())==entropy::publish::ReadResult::Gap);render.close();dsp.stop();source.close();s=session(1);CHECK(source.create(s,error));CHECK(dsp.attach(s,error));DspLocalDemand monitor;StageBusProgramSnapshot program{};program.required_inputs=3;program.music_program_active=1;CHECK(monitor.attach(dsp.outputSession(),program,1,error));auto base=qpc();uint64_t seq=0;
auto localCycle=[&](){++seq;auto now=base+(seq-1)*frequency()*1024/192000+100;bool result=monitor.step(now)!=DspStepResult::Fault;result &=dsp.step(now)!=DspStepResult::Fault;StageBusFrameRequest r{};result &=source.tryReceiveRequest(r)==StageBusIoResult::Ok;auto b=header(r);result &=source.tryPublish(b,input.in.data())==StageBusIoResult::Ok;result &=dsp.step(now)==DspStepResult::Published;result &=monitor.step(now+200)==DspStepResult::Published;return result;};for(unsigned i=0;i<100;i++)CHECK(localCycle());allocation_audit::reset();ok=true;for(unsigned i=0;i<1000;i++){allocation_audit::CycleScope scope;allocation_audit::audioPhase();ok &=localCycle();}counts=allocation_audit::snapshot();allocation_audit::report();CHECK(ok);CHECK(counts.cppNew==0 && counts.cppDelete==0 && counts.heapAlloc==0 && counts.heapFree==0 && counts.wait==0 && counts.lock==0 && counts.io==0);monitor.beginQuiesce();monitor.close();
std::cout<<"PASS 1000 external-demand + 1000 local-monitor warmed steady-state request/consume/executor/publish cycles; V2 commit/full-delivery covered new/free/wait/lock/disk zero; lifecycle/peerAlive/worker scheduling excluded\n";}

