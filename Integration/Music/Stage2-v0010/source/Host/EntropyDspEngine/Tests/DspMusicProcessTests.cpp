#include "MusicTestSupport.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <atomic>
#include <fstream>
#include <thread>
struct Child {
 PROCESS_INFORMATION process{};
 ~Child(){if(process.hProcess){if(WaitForSingleObject(process.hProcess,0)==WAIT_TIMEOUT){TerminateProcess(process.hProcess,91);WaitForSingleObject(process.hProcess,2000);}CloseHandle(process.hThread);CloseHandle(process.hProcess);}}
 bool start(const std::wstring& exe,std::wstring args){STARTUPINFOW info{};info.cb=sizeof(info);auto command=L"\""+exe+L"\" "+args;return CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&info,&process);}
 bool exited(){return WaitForSingleObject(process.hProcess,0)==WAIT_OBJECT_0;}
 bool success(){DWORD code=99;return GetExitCodeProcess(process.hProcess,&code) && code==0;}
};
int main(int argc,char** argv){
 QCoreApplication app(argc,argv);CHECK(argc==2);auto executable=std::filesystem::path(argv[1]).wstring();
 QTemporaryDir tmp;CHECK(tmp.isValid());auto root=std::filesystem::path(tmp.path().toStdWString());
 auto endpoint=musicEndpoint(L".production");auto s=session(3);s.producer_pid=GetCurrentProcessId();std::string error;
 entropy::publish::Publisher source;CHECK(source.start(endpoint,error));source.setSession(s);
 std::atomic<uint64_t> produced{0};
 std::jthread worker([&](std::stop_token stop){Planes<2> planes;planes.data[0].fill(.1);planes.data[1].fill(.075);uint64_t seq=71;while(!stop.stop_requested()){source.service();source.publish(musicHeader(s,seq++),planes.in.data());produced++;std::this_thread::sleep_for(std::chrono::milliseconds(6));}});
 // Exercise the packaged executable, its manifest checks, native DLLs, controller and IPC.
 for(unsigned attempt=0;attempt<2;attempt++){
  auto stop=root/("stop-"+std::to_string(attempt)),diag=root/("diag-"+std::to_string(attempt)+".json");
  auto args=L"--headless --automatic --runtime-root \""+root.wstring()+L"\" --link-endpoint \""+endpoint+L"\" --stop-file \""+stop.wstring()+L"\" --diagnostic-file \""+diag.wstring()+L"\"";
  Child child;CHECK(child.start(executable,args));
  entropy::publish::Subscriber output;bool connected=false;auto deadline=qpc()+frequency()*8;
  while(qpc()<deadline && !child.exited()){if(output.connect(endpoint+L".StagePublish.V2",error)){connected=true;break;}Sleep(20);}
  if(!connected)std::cerr<<error<<"\n";CHECK(connected);CHECK(output.session().layout_id==4 && output.session().channels==3);
  CHECK(!std::memcmp(output.session().session_id,s.session_id,16));
  Planes<3> planes;StageBusBlockHeader b{};unsigned read=0;deadline=qpc()+frequency()*3;
  while(read<8 && qpc()<deadline){output.heartbeat(qpc());auto result=output.read(b,planes.out.data());if(result==entropy::publish::ReadResult::Block){++read;CHECK(b.validity_mask==3 && !b.source_generation[2]);}else CHECK(result==entropy::publish::ReadResult::Empty);Sleep(1);}
  CHECK(read==8);output.close();
  std::ofstream(stop).put('1');CHECK(WaitForSingleObject(child.process.hProcess,4000)==WAIT_OBJECT_0);CHECK(child.success());
  QFile report(QString::fromStdWString(diag.wstring()));CHECK(report.open(QIODevice::ReadOnly));auto object=QJsonDocument::fromJson(report.readAll()).object();
  CHECK(object.value("blocks_out").toInteger()>=8);CHECK(object.value("input_channels").toInt()==2 && object.value("output_channels").toInt()==3);
  CHECK(!object.contains("cinema_latency_samples"));CHECK(object.value("center_loaded").toBool() && object.value("lrc_loaded").toBool());
  auto before=produced.load();Sleep(40);CHECK(produced.load()>before); // Source continues while Stage2 is stopped.
 }
 worker.request_stop();worker.join();source.stop();
 std::cout<<"PASS packaged Stage2 MUSIC native DLL/controller/IPC, graceful stop/restart, source UUID and independent producer\n";
}
