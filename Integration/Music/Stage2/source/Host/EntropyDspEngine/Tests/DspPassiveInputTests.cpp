#include "TestSupport.h"
#include "DspController.h"
#include "StagePublishV2.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <atomic>
#include <thread>

// A V1-demand runtime never produces from this real passive publication. The
// test also catches renumbered blocks, changed FP64 values and upstream stops.
int main(int argc,char** argv){
 QCoreApplication app(argc,argv);
 const auto endpoint=entropy::publish::endpointName()+L".passive-input."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(qpc());
 const auto outputEndpoint=endpoint+L".StagePublish.V2";
 auto s=session(MIX_CINEMA10_V1);s.producer_pid=GetCurrentProcessId();
 std::string error;entropy::publish::Publisher source;CHECK(source.start(endpoint,error));
 source.setSession(s);std::atomic<uint64_t> produced{0},epoch{1};std::atomic<bool> jump{false};
 std::jthread worker([&](std::stop_token stop){
  Planes<10> input;for(unsigned c=0;c<10;c++)for(unsigned f=0;f<1024;f++)input.data[c][f]=c*1024+f+0.25;
  uint64_t seq=70;
  while(!stop.stop_requested()){
   source.service();if(jump.exchange(false))seq+=100;
   auto b=header(request(s,++seq));b.stream_epoch=epoch.load();b.control_revision=29;
   source.publish(b,input.in.data());++produced;Sleep(5);
  }
 });
 QTemporaryDir tmp;DspController runtime;CHECK(runtime.startAutomatic(std::filesystem::path(tmp.path().toStdWString()),error,endpoint));
 const auto advance=[&](uint64_t count){const auto deadline=qpc()+frequency()*4;while(qpc()<deadline){if(runtime.snapshot().diag.blocksOut>=count)return true;Sleep(1);}std::cerr<<runtime.snapshot().error<<"\n";return false;};
 CHECK(advance(4));entropy::publish::Subscriber output;CHECK(output.connect(outputEndpoint,error));
 Planes<11> planes;StageBusBlockHeader b{};
 const auto read=[&](){const auto deadline=qpc()+frequency();auto r=entropy::publish::ReadResult::Empty;while(qpc()<deadline){output.heartbeat(qpc());r=output.read(b,planes.out.data());if(r!=entropy::publish::ReadResult::Empty)return r;Sleep(1);}return r;};
 CHECK(read()==entropy::publish::ReadResult::Block);
 CHECK(b.block_sequence>70 && b.first_frame==(b.block_sequence-1)*1024);
 CHECK(b.stream_epoch==1 && b.control_revision==29);
 CHECK(b.source_generation[0]==7 && b.source_generation[1]==9 && b.source_generation[2]==13 && b.music_session_revision==11);
 CHECK(b.validity_mask==7 && !std::memcmp(b.session_id,s.session_id,16));
 for(unsigned c=0;c<11;c++)for(unsigned f=0;f<1024;f++)CHECK(bits(planes.data[c][f])==bits(c==2?0.0:(c<2?c:c-1)*1024+f+0.25));
 auto before=runtime.snapshot().diag.blocksOut;jump=true;CHECK(advance(before+8));
 CHECK(read()==entropy::publish::ReadResult::Disconnected);output.close();
 CHECK(runtime.snapshot().diag.missing>0 && runtime.snapshot().diag.faults>0);
 CHECK(output.connect(outputEndpoint,error));CHECK(read()==entropy::publish::ReadResult::Block);CHECK(b.block_sequence>170);output.close();
 before=runtime.snapshot().diag.blocksOut;epoch=2;CHECK(advance(before+8));
 CHECK(output.connect(outputEndpoint,error));CHECK(read()==entropy::publish::ReadResult::Block);CHECK(b.stream_epoch==2 && b.control_revision==29);output.close();
 // A local stop/restart closes only this subscription; source identity/cadence survives.
 runtime.stop();const auto sourceBefore=produced.load();const auto deadline=qpc()+frequency();
 while(produced.load()<sourceBefore+6 && qpc()<deadline)Sleep(1);CHECK(produced.load()>=sourceBefore+6);
 before=runtime.snapshot().diag.blocksOut;CHECK(runtime.startAutomatic(std::filesystem::path(tmp.path().toStdWString()),error,endpoint));CHECK(advance(before+4));
 CHECK(output.connect(outputEndpoint,error));CHECK(read()==entropy::publish::ReadResult::Block);CHECK(b.stream_epoch==2 && !std::memcmp(b.session_id,s.session_id,16));output.close();
 runtime.stop();worker.request_stop();worker.join();source.stop();
 std::cout<<"PASS passive real Stage1 transport -> Stage2 -> publication; exact FP64 and identity; gap/epoch/restart remain local\n";
}
