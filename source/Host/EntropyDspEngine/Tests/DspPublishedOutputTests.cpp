#include "TestSupport.h"
#include "DspController.h"
#include "StageLinkServer.h"
#include "StagePublishV2.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <atomic>
#include <thread>

int main(int argc,char** argv){
 QCoreApplication app(argc,argv);
 if(argc==4 && std::string(argv[1])=="--subscriber"){
  entropy::publish::Subscriber subscriber;std::string error;CHECK(subscriber.connect(QString::fromUtf8(argv[2]).toStdWString(),error));
  auto ready=OpenEventW(EVENT_MODIFY_STATE,FALSE,QString::fromUtf8(argv[3]).toStdWString().c_str());CHECK(ready);CHECK(SetEvent(ready));CloseHandle(ready);
  for(;;){subscriber.heartbeat(qpc());Sleep(5);}
 }
 using namespace entropy::link;
 auto endpoint=endpointName()+L".publication."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(qpc());
 auto publishEndpoint=endpoint+L".StagePublish.V2";
 auto s=session(1);std::string error;StageBusWin32 source;CHECK(source.create(s,error));
 StageLinkServer sourceLink(endpoint);CHECK(sourceLink.start(error));std::atomic<uint64_t> releases{0};std::atomic<bool> sourceFault{false};
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
   if(io==StageBusIoResult::Ok){auto b=header(frame);if(source.tryPublish(b,input.in.data())!=StageBusIoResult::Ok)sourceFault=true;}
   Sleep(1);
  }
 });
 QTemporaryDir tmp;DspController controller;CHECK(controller.startAutomatic(std::filesystem::path(tmp.path().toStdWString()),error,endpoint));
 auto advance=[&](uint64_t count){auto deadline=qpc()+frequency()*3;while(qpc()<deadline){auto snapshot=controller.snapshot();if(snapshot.state==DspStateCode::Fault){std::cerr<<snapshot.error<<"\n";return false;}if(snapshot.diag.blocksOut>=count)return true;Sleep(1);}return false;};
 CHECK(advance(8));entropy::publish::Subscriber subscriber;
 auto connected=subscriber.connect(publishEndpoint,error);if(!connected)std::cerr<<error<<"\n";CHECK(connected);CHECK(subscriber.session().layout_id==2);
 Planes<11> output;StageBusBlockHeader block{};auto deadline=qpc()+frequency();entropy::publish::ReadResult result;
 do{subscriber.heartbeat(qpc());result=subscriber.read(block,output.out.data());if(result==entropy::publish::ReadResult::Empty)Sleep(1);}while(result==entropy::publish::ReadResult::Empty && qpc()<deadline);
 CHECK(result==entropy::publish::ReadResult::Block);CHECK(block.block_sequence>8);CHECK(block.first_frame==(block.block_sequence-1)*1024);CHECK(block.stream_epoch==1 && block.control_revision==1);
 CHECK(!std::memcmp(block.session_id,s.session_id,16));CHECK(block.source_generation[0]==7 && block.source_generation[1]==9 && block.source_generation[2]==13 && block.music_session_revision==11);
 CHECK(block.qpc_timestamp>0);
 CHECK(block.frames==1024 && block.validity_mask==7 && block.discontinuity_mask==0 && block.reserved==0);
 for(unsigned c=0;c<11;c++)for(unsigned f=0;f<1024;f++)CHECK(bits(output.data[c][f])==bits(c==2?0.0:(c<2?c:c-1)*1024+f+0.25));
 subscriber.close();auto before=controller.snapshot().diag.blocksOut;CHECK(advance(before+24));CHECK(releases==0);CHECK(!sourceFault);CHECK(!std::memcmp(controller.snapshot().outputSession.session_id,s.session_id,16));
 // Leave a real subscription unread until its bounded queue fills. It affects
 // this subscriber only; Stage2 must continue producing under the same UUID.
 CHECK(subscriber.connect(publishEndpoint,error));before=controller.snapshot().diag.blocksOut;CHECK(advance(before+40));CHECK(controller.snapshot().diag.publishDrops>0);CHECK(subscriber.read(block,output.out.data())==entropy::publish::ReadResult::Gap);subscriber.close();CHECK(releases==0);CHECK(!sourceFault);
 // Kill only a second subscriber process. No upstream lease belongs to it.
 auto readyName=L"Local\\Entropy.Stage2.PublicationTest.Ready."+std::to_wstring(GetCurrentProcessId());auto ready=CreateEventW(nullptr,TRUE,FALSE,readyName.c_str());CHECK(ready);
 auto exe=QCoreApplication::applicationFilePath().toStdWString();std::wstring command=L"\""+exe+L"\" --subscriber \""+publishEndpoint+L"\" \""+readyName+L"\"";
 STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
 CHECK(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process));
 CHECK(WaitForSingleObject(ready,2000)==WAIT_OBJECT_0);CloseHandle(ready);CHECK(WaitForSingleObject(process.hProcess,0)==WAIT_TIMEOUT);CHECK(TerminateProcess(process.hProcess,23));CHECK(WaitForSingleObject(process.hProcess,2000)==WAIT_OBJECT_0);CloseHandle(process.hThread);CloseHandle(process.hProcess);
 before=controller.snapshot().diag.blocksOut;CHECK(advance(before+32));CHECK(releases==0);CHECK(!sourceFault);CHECK(!std::memcmp(controller.snapshot().outputSession.session_id,s.session_id,16));
 CHECK(subscriber.connect(publishEndpoint,error));deadline=qpc()+frequency();do{subscriber.heartbeat(qpc());result=subscriber.read(block,output.out.data());if(result==entropy::publish::ReadResult::Empty)Sleep(1);}while(result==entropy::publish::ReadResult::Empty && qpc()<deadline);CHECK(result==entropy::publish::ReadResult::Block);CHECK(block.block_sequence>before);CHECK(!std::memcmp(block.session_id,s.session_id,16));subscriber.close();
 controller.stop();sourceWorker.request_stop();sourceWorker.join();
 std::cout<<"PASS exact published FP64/metadata; absent, disconnected, full and crashed subscribers preserve advancing Stage2 and source UUID; fresh subscription receives current output\n";
}
