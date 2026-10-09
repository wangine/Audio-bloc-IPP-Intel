#include "SourceController.h"
#include "TestSupport.h"
#include <QCoreApplication>
#include <QTemporaryDir>
using namespace entropy::host;
template<class Predicate> bool waitFor(Predicate p){auto until=GetTickCount64()+2000;while(!p()){if(GetTickCount64()>until)return false;Sleep(1);}return true;}
int sourceApplicationProof(int argc,char** argv){
 QCoreApplication app(argc,argv);QTemporaryDir tmp;std::string error;
 auto root=std::filesystem::path(tmp.path().toStdWString());SourceState state(root);SourceStateSnapshot saved;
 saved.manualMuteMask=5;saved.engines={1,7,1};CHECK(state.save(saved,error));SourceController c;
 CHECK(c.start(TEST_MODULE_ROOT,root,error,true));SourceController duplicate;
 CHECK(!duplicate.start(TEST_MODULE_ROOT,root/"duplicate",error,true));
 CHECK(waitFor([&]{return c.blocks()>=8;}));CHECK(!c.snapshot().capturing);
 CHECK(c.snapshot().state.engines == (std::array<uint32_t,3>{0,0,2}));
 EngineTelemetrySnapshot fixedTelemetry{};CHECK(c.telemetry(fixedTelemetry));
 CHECK(fixedTelemetry.srcEngineCount==1 && fixedTelemetry.srcEngines[0].engine==0);
 SourceCommand removed;removed.type=SourceCommand::Engine;removed.role=0;removed.value=1;
 CHECK(c.command(removed));CHECK(waitFor([&]{return c.snapshot().reason=="Stage 1 uses fixed r8brain SRC";}));
 CHECK(c.snapshot().faults==0 && c.snapshot().state.engines[0]==0);
 SourceCommand mode;mode.type=SourceCommand::BusMode;mode.value=1;CHECK(c.command(mode));
 CHECK(waitFor([&]{return c.snapshot().external;}));auto initial=c.snapshot();
 StageBusWin32 peer;CHECK(peer.open(initial.session,error));peer.heartbeat(qpc());
 Planes<10> audio;StageBusBlockHeader block{};StageBusFrameRequest last{};
 auto receive=[&]{StageBusIoResult io=StageBusIoResult::Empty;return waitFor([&]{peer.heartbeat(qpc());io=peer.tryConsume(block,audio.out.data());return io!=StageBusIoResult::Empty;})&&io==StageBusIoResult::Ok;};
 for(uint64_t seq=1;seq<=100;++seq){
  auto r=request(initial.session,seq);r.control_revision=initial.state.revision;
  // First half: unchanged legacy API. Second half: Source-only envelope with opaque baggage.
  if(seq<=50){r.program.hardware_armed=seq%2;r.program.manual_mute_mask=r.program.effective_mute_mask=2;}
  else {std::memset(&r.program,0xa5,sizeof(r.program));}
  CHECK((seq<=50?peer.trySendRequest(r):peer.trySendSourceRequest(r))==StageBusIoResult::Ok);
  CHECK(receive());CHECK(block.block_sequence==seq && block.validity_mask==0);
  for(auto& channel:audio.data)for(double v:channel)CHECK(v==0.0);
  CHECK(c.snapshot().state.manualMuteMask==5 && c.snapshot().state.revision==initial.state.revision);
  last=r;
 }
 auto once=c.blocks();last.program={};CHECK(peer.trySendSourceRequest(last)==StageBusIoResult::Ok);
 StageBusIoResult io;CHECK(waitFor([&]{io=peer.tryReceiveResponse(block);return io!=StageBusIoResult::Empty;}));
 CHECK(io==StageBusIoResult::Ok && c.blocks()==once && c.snapshot().faults==0);
 // Load is local, even after the consumer asserted legacy hardware_armed=1.
 auto r=request(initial.session,101);r.control_revision=initial.state.revision;r.program.hardware_armed=1;
 CHECK(peer.trySendRequest(r)==StageBusIoResult::Ok);CHECK(receive());
 SourceCommand load;load.type=SourceCommand::Load;CHECK(c.command(load));
 CHECK(waitFor([&]{return c.snapshot().state.revision>initial.state.revision;}));
 CHECK(c.snapshot().state.manualMuteMask==5 && c.snapshot().faults==0);
 // Local mute becomes authoritative without a consumer echo and can be saved immediately.
 c.mute(2);SourceCommand save;save.type=SourceCommand::Save;CHECK(c.command(save));
 SourceStateSnapshot persisted;CHECK(waitFor([&]{return state.load(persisted,error)&&persisted.manualMuteMask==2;}));
 SourceCommand retry;retry.type=SourceCommand::Retry;CHECK(c.command(retry));
 CHECK(waitFor([&]{return c.snapshot().reason.find("OFFLINE")!=std::string::npos;}));
 CHECK(c.snapshot().faults==0);
 r=request(initial.session,102);r.control_revision=c.snapshot().state.revision;r.program.hardware_armed=1;
 CHECK(peer.trySendRequest(r)==StageBusIoResult::Ok);CHECK(receive());
 CHECK(block.discontinuity_mask==7 && c.snapshot().state.manualMuteMask==2);
 peer.heartbeat(qpc());mode.value=0;CHECK(c.command(mode));CHECK(waitFor([&]{return !c.snapshot().external;}));
 CHECK(c.snapshot().faults==0);peer.close();
 // A full healthy transport refuses reconfiguration; a faulted one can abandon its pending request.
 mode.value=1;CHECK(c.command(mode));CHECK(waitFor([&]{return c.snapshot().external;}));
 auto stalled=c.snapshot();CHECK(peer.open(stalled.session,error));peer.heartbeat(qpc());
 for(uint64_t seq=1;seq<=5;++seq){
  auto pending=request(stalled.session,seq);pending.control_revision=stalled.state.revision;
  CHECK(peer.trySendRequest(pending)==StageBusIoResult::Ok);
  if(seq<5)CHECK(waitFor([&]{return c.snapshot().sequence==seq;}));
 }
 Sleep(30);
 // Local SRC control must progress even while the unchanged V1 ring is full.
 SourceCommand selected;selected.type=SourceCommand::Select;selected.key=SourceStartupPolicy::musicKey();selected.role=0;selected.value=1;CHECK(c.command(selected));
 CHECK(waitFor([&]{return c.snapshot().reason=="MUSIC selection unavailable while capture is offline";}));
 auto beforeLocal=c.snapshot();SourceCommand engine;engine.type=SourceCommand::Engine;engine.role=0;engine.value=beforeLocal.state.engines[0];CHECK(c.command(engine));
 CHECK(waitFor([&]{return c.snapshot().state.revision>beforeLocal.state.revision;}));
 CHECK(!std::memcmp(c.snapshot().session.session_id,stalled.session.session_id,16));
 CHECK(c.command(retry));Sleep(40);
 CHECK(!std::memcmp(c.snapshot().session.session_id,stalled.session.session_id,16));
 CHECK(waitFor([&]{peer.heartbeat(qpc());return c.snapshot().faults>stalled.faults;}));
 CHECK(c.command(retry));
 CHECK(waitFor([&]{return std::memcmp(c.snapshot().session.session_id,stalled.session.session_id,16)!=0;}));
 CHECK(c.snapshot().sequence==0);peer.close();c.stop();
 CHECK(c.start(TEST_MODULE_ROOT,root,error,true));CHECK(waitFor([&]{return c.blocks()>once;}));c.stop();
 std::cout<<"PASS 100 generic/legacy demands, invisible Render payload, local Load/mute/Save/Retry, owner handoff and exclusive owner\n";
return 0;
}

#ifndef ENTROPY_SOURCE_INDEPENDENCE_AGGREGATE
int main(int argc,char** argv){return sourceApplicationProof(argc,argv);}
#endif
