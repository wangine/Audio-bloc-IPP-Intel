#include "TestSupport.h"
#include "DspDownstreamLink.h"
#include <atomic>
#include <future>
#include <thread>

// A V1 takeover must never retire the real upstream lease or disturb the
// continuously advancing local monitor. Uses real pipes and StageBus mappings.
int main(){
 using namespace entropy::link;
 auto endpoint=endpointName()+L".isolation."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(qpc());
 auto s=session(1);std::string error;StageBusWin32 source;CHECK(source.create(s,error));
 StageLinkServer sourceLink(endpoint);CHECK(sourceLink.start(error));
 std::atomic<uint64_t> releases{0},sourceBlocks{0};std::atomic<bool> sourceFault{false};
 std::jthread sourceWorker([&](std::stop_token stop){
  Planes<10> input;for(unsigned c=0;c<10;c++)for(unsigned f=0;f<1024;f++)input.data[c][f]=c*1024+f+0.25;
  while(!stop.stop_requested()){
   source.heartbeat(qpc());StageLinkRequest req{};uint32_t pid=0;
   if(sourceLink.tryTake(req,pid)){
    StageLinkResponse response{};response.sourcePid=GetCurrentProcessId();response.session=s;response.status=StageLinkStatus::Ok;
    response.controlRevision=1;response.program.required_inputs=3;response.program.music_program_active=1;response.lease[0]=42;
    std::memcpy(response.nonce,req.nonce,16);if(req.action==StageLinkAction::Release)++releases;sourceLink.complete(response);
   }
   StageBusFrameRequest frame{};auto io=source.tryReceiveRequest(frame);
   if(io==StageBusIoResult::Fault)sourceFault=true;
   if(io==StageBusIoResult::Ok){auto b=header(frame);if(source.tryPublish(b,input.in.data())!=StageBusIoResult::Ok)sourceFault=true;else ++sourceBlocks;}
   Sleep(1);
  }
 });
 DspStageRuntime runtime;DspLocalDemand local;DspAutoLink autoLink(endpoint);DspDownstreamLink downstream(endpoint+L".Downstream.V1");CHECK(downstream.start(error));
 auto initial=autoLink.poll(qpc(),runtime,local,error,true);if(initial!=DspAutoLinkState::Receiving)std::cerr<<error<<"\n";CHECK(initial==DspAutoLinkState::Receiving);
 auto uuid=runtime.outputSession();
 auto pump=[&]{downstream.service(autoLink,runtime,local,error);return autoLink.poll(qpc(),runtime,local,error,true);};
 auto advance=[&](uint64_t blocks){auto until=qpc()+frequency()*2;while(runtime.diagnostics().blocksOut<blocks && qpc()<until){if(pump()!=DspAutoLinkState::Receiving)return false;Sleep(1);}return runtime.diagnostics().blocksOut>=blocks;};
 CHECK(advance(4));
 StageLinkClient client(endpoint+L".Downstream.V1");
 for(auto action:{StageLinkAction::Attach,StageLinkAction::Release}){
  auto req=makeRequest(action);std::memcpy(req.expectedSession,uuid.session_id,16);StageLinkResponse response{};std::string rpcError;
  auto rpc=std::async(std::launch::async,[&]{return client.transact(req,response,1500,rpcError);});
  auto until=qpc()+frequency()*2;while(rpc.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready && qpc()<until){auto state=pump();if(state!=DspAutoLinkState::Receiving)std::cerr<<"upstream releases="<<releases<<"; "<<error<<"\n";CHECK(state==DspAutoLinkState::Receiving);Sleep(1);}
  CHECK(rpc.get());CHECK(response.status==StageLinkStatus::Refused);CHECK(!downstream.external());CHECK(releases==0);
  CHECK(!std::memcmp(uuid.session_id,runtime.outputSession().session_id,16));CHECK(advance(runtime.diagnostics().blocksOut+4));
 }
 CHECK(sourceBlocks>=12);CHECK(!sourceFault);CHECK(!runtime.faulted());
 CHECK(autoLink.finish(runtime,local,error));sourceWorker.request_stop();sourceWorker.join();
 std::cout<<"PASS V1 takeover/release refused; local demand advances with original source UUID and no upstream release\n";
}


