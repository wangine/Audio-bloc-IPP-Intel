#include "TestSupport.h"
#include "DspStageRuntime.h"
#include <filesystem>
#include <fstream>
namespace fs=std::filesystem;
StageBusSessionHeader knownSession(uint64_t token,unsigned layout){uint8_t uuid[16]{};std::memcpy(uuid,&token,8);uuid[15]=53;return makeSession(layout,uuid,frequency());}
struct Peer {PROCESS_INFORMATION p{};~Peer(){if(p.hProcess){if(WaitForSingleObject(p.hProcess,0)==WAIT_TIMEOUT)TerminateProcess(p.hProcess,98);CloseHandle(p.hProcess);CloseHandle(p.hThread);}}bool start(const fs::path& own,const wchar_t* role,uint64_t token,const fs::path& root){auto command=L"\""+own.wstring()+L"\" "+role+L" "+std::to_wstring(token)+L" \""+root.wstring()+L"\"";STARTUPINFOW si{};si.cb=sizeof(si);return CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&p);}bool kill(){return TerminateProcess(p.hProcess,77) && WaitForSingleObject(p.hProcess,3000)==WAIT_OBJECT_0;}};
int main(int argc,char** argv){std::string error;
 if(argc>1){auto token=std::stoull(argv[2]);fs::path root=argv[3];StageBusWin32 bus;bool source=std::string(argv[1])=="--source";auto s=knownSession(token,source?1:2);if(source){CHECK(bus.create(s,error));}else {CHECK(bus.open(s,error));}std::ofstream(root/"ready").put('1');Planes<10> audio;auto end=qpc()+frequency()*10;uint64_t next=1;
  while(qpc()<end){bus.heartbeat(qpc());if(source){StageBusFrameRequest req{};auto io=bus.tryReceiveRequest(req);CHECK(io!=StageBusIoResult::Fault);if(io==StageBusIoResult::Ok){auto b=header(req);CHECK(bus.tryPublish(b,audio.in.data())==StageBusIoResult::Ok);}}
   else if(next<=5){auto* index=reinterpret_cast<volatile LONG64*>(static_cast<unsigned char*>(bus.address())+256);if(InterlockedCompareExchange64(index,0,0)>=static_cast<LONG64>(next-1)){CHECK(bus.trySendRequest(request(s,next))==StageBusIoResult::Ok);next++;}}
   Sleep(1);
  }return 0;
 }
 wchar_t ownName[MAX_PATH];GetModuleFileNameW(nullptr,ownName,MAX_PATH);fs::path own=ownName;
 for(unsigned role=0;role<2;role++){
  auto token=qpc();auto s=knownSession(token,1);auto root=fs::temp_directory_path()/("stage2-peer-death-"+std::to_string(token));fs::create_directories(root);StageBusWin32 localSource,localRender;DspStageRuntime dsp;Peer child;Planes<10> audio;
  if(role==0){CHECK(child.start(own,L"--source",token,root));auto deadline=qpc()+frequency()*5;while(!fs::exists(root/"ready")){CHECK(qpc()<deadline);Sleep(1);}CHECK(dsp.attach(s,error));CHECK(localRender.open(dsp.outputSession(),error));}
  else {CHECK(localSource.create(s,error));CHECK(dsp.attach(s,error));CHECK(child.start(own,L"--render",token,root));}
  uint64_t sent=0;auto deadline=qpc()+frequency()*5;
  while(dsp.diagnostics().blocksIn<5){CHECK(qpc()<deadline);dsp.heartbeat(qpc());
   if(role==0){localRender.heartbeat(qpc());if(sent<5 && dsp.diagnostics().blocksIn==sent){CHECK(localRender.trySendRequest(request(s,++sent))==StageBusIoResult::Ok);}}
   else localSource.heartbeat(qpc());
   CHECK(dsp.step(qpc())!=DspStepResult::Fault);
   if(role==1){StageBusFrameRequest req{};auto io=localSource.tryReceiveRequest(req);CHECK(io!=StageBusIoResult::Fault);if(io==StageBusIoResult::Ok){auto b=header(req);CHECK(localSource.tryPublish(b,audio.in.data())==StageBusIoResult::Ok);}}
   Sleep(1);
  }
  CHECK(dsp.pending() && dsp.diagnostics().blocksOut==4);CHECK(child.kill());if(role==0)localRender.heartbeat(qpc());else localSource.heartbeat(qpc());CHECK(!dsp.peersAlive(qpc()));CHECK(dsp.faulted());CHECK(dsp.diagnostics().blocksOut==4);dsp.stop();
 }
 std::cout<<"PASS actual Source-peer and downstream-peer processes terminated with fifth block pending; both buses fault, no extra publish or silent drop\n";
}
