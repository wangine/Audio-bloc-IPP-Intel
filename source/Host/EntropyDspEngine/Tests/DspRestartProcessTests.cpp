#include "TestSupport.h"
#include "DspSessionDiscovery.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <filesystem>
#include <fstream>
namespace fs=std::filesystem;
struct Child {PROCESS_INFORMATION p{};~Child(){if(p.hProcess){if(WaitForSingleObject(p.hProcess,0)==WAIT_TIMEOUT)TerminateProcess(p.hProcess,98);CloseHandle(p.hProcess);CloseHandle(p.hThread);}}bool start(const fs::path& exe,const fs::path& root){auto command=L"\""+exe.wstring()+L"\" --headless --test-harness --runtime-root \""+root.wstring()+L"\" --source-session \""+(root/"source.json").wstring()+L"\" --stop-file \""+(root/"stop").wstring()+L"\"";STARTUPINFOW si{};si.cb=sizeof(si);return CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&p);}bool wait(DWORD expected){if(WaitForSingleObject(p.hProcess,5000)!=WAIT_OBJECT_0)return false;DWORD code=99;GetExitCodeProcess(p.hProcess,&code);return code==expected;}};
int main(int argc,char** argv){QCoreApplication app(argc,argv);QTemporaryDir dir;CHECK(dir.isValid());auto root=fs::path(dir.path().toStdWString());auto exe=fs::path(QCoreApplication::applicationDirPath().toStdWString())/"DspTestEngine.exe";std::string error;StageBusWin32 source;auto s=session(1);
 for(unsigned cycle=0;cycle<2;cycle++){
  CHECK(source.create(s,error));CHECK(DspSessionDiscovery::write(root/"source.json",s,"EXTERNAL",error));Child child;CHECK(child.start(exe,root));StageBusWin32 render;auto downstream=makeSession(2,s.session_id,s.qpc_frequency);auto deadline=qpc()+frequency()*5;while(!render.open(downstream,error)){source.heartbeat(qpc());CHECK(qpc()<deadline);Sleep(1);}Planes<10> input;Planes<11> output;auto r=request(downstream,1);CHECK(render.trySendRequest(r)==StageBusIoResult::Ok);StageBusFrameRequest adopted{};deadline=qpc()+frequency();while(source.tryReceiveRequest(adopted)==StageBusIoResult::Empty){CHECK(qpc()<deadline);source.heartbeat(qpc());render.heartbeat(qpc());Sleep(1);}CHECK(!std::memcmp(&r,&adopted,56));CHECK(adopted.program.required_inputs==3 && adopted.program.music_program_active==1 && adopted.program.analyze_music_final==1);for(unsigned c=0;c<10;c++)for(unsigned i=0;i<1024;i++)input.data[c][i]=cycle*10000+c*1024+i;auto b=header(adopted);CHECK(source.tryPublish(b,input.in.data())==StageBusIoResult::Ok);StageBusBlockHeader got{};StageBusIoResult result;deadline=qpc()+frequency();while((result=render.tryConsume(got,output.out.data()))==StageBusIoResult::Empty){CHECK(qpc()<deadline);source.heartbeat(qpc());render.heartbeat(qpc());Sleep(1);}CHECK(result==StageBusIoResult::Ok);CHECK(!std::memcmp(&b,&got,128));for(unsigned c=0;c<10;c++)CHECK(!std::memcmp(input.in[c],output.in[c<2?c:c+1],8192));for(double x:output.data[2])CHECK(bits(x)==0);std::ofstream(root/"stop").put('1');CHECK(child.wait(0));render.close();
  if(cycle==0){Child stale;CHECK(stale.start(exe,root));CHECK(stale.wait(2));auto previous=s;s=session(1);CHECK(std::memcmp(s.session_id,previous.session_id,16));}
  source.close();fs::remove(root/"stop");
 }
 std::cout<<"PASS actual S2 process STOP/restart refuses old still-owned UUID, new publisher UUID recovers at seq1/epoch1; transition is not continuous audio\n";
}
