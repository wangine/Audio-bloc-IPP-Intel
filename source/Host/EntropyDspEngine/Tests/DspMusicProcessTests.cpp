#include "MusicTestSupport.h"
#include "DspState.h"
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
 QCoreApplication app(argc,argv);CHECK(argc==3);auto executable=std::filesystem::path(argv[1]).wstring();auto baselineExecutable=std::filesystem::path(argv[2]).wstring();
 QTemporaryDir tmp;CHECK(tmp.isValid());auto root=std::filesystem::path(tmp.path().toStdWString());
 auto endpoint=musicEndpoint(L".production");auto s=session(3);s.producer_pid=GetCurrentProcessId();std::string error;
 entropy::publish::Publisher source;CHECK(source.start(endpoint,error));source.setSession(s);
 std::atomic<uint64_t> produced{0};
 std::jthread worker([&](std::stop_token stop){Planes<2> planes;planes.data[0].fill(.1);planes.data[1].fill(.075);uint64_t seq=71;while(!stop.stop_requested()){source.service();source.publish(musicHeader(s,seq++),planes.in.data());produced++;std::this_thread::sleep_for(std::chrono::milliseconds(6));}});
 // Exercise the packaged executable, its manifest checks, native DLLs, controller and IPC.
 Planes<3> offCandidate;
 for(unsigned attempt=0;attempt<3;attempt++){
  {DspState profile;profile.recoverySettings.enabled=attempt==1;CHECK(profile.save(root/"state/Stage2State.json",error));}
  auto stop=root/("stop-"+std::to_string(attempt)),diag=root/("diag-"+std::to_string(attempt)+".json");
  auto args=L"--headless --automatic --runtime-root \""+root.wstring()+L"\" --link-endpoint \""+endpoint+L"\" --stop-file \""+stop.wstring()+L"\" --diagnostic-file \""+diag.wstring()+L"\"";
  Child child;CHECK(child.start(attempt==2?baselineExecutable:executable,args));
  entropy::publish::Subscriber output;bool connected=false;auto deadline=qpc()+frequency()*8;
  while(qpc()<deadline && !child.exited()){if(output.connect(endpoint+L".StagePublish.V2",error)){connected=true;break;}Sleep(20);}
  if(!connected)std::cerr<<error<<"\n";CHECK(connected);CHECK(output.session().layout_id==4 && output.session().channels==3);
  CHECK(!std::memcmp(output.session().session_id,s.session_id,16));
  Planes<3> planes;StageBusBlockHeader b{};unsigned read=0;deadline=qpc()+frequency()*3;
  while(read<8 && qpc()<deadline){output.heartbeat(qpc());auto result=output.read(b,planes.out.data());if(result==entropy::publish::ReadResult::Block){++read;CHECK(b.validity_mask==3 && !b.source_generation[2]);}else CHECK(result==entropy::publish::ReadResult::Empty);Sleep(1);}
  CHECK(read==8);
  if(attempt==0)offCandidate=planes;
  // Compare the stereo slot; the independent Center history depends on how
  // many asynchronous source blocks were consumed before this observation.
  if(attempt==2)for(unsigned channel=0;channel<2;channel++){
   CHECK(!std::memcmp(offCandidate.data[channel].data(),planes.data[channel].data(),1024*sizeof(double)));
  }
  output.close();
  std::ofstream(stop).put('1');CHECK(WaitForSingleObject(child.process.hProcess,4000)==WAIT_OBJECT_0);CHECK(child.success());
  QFile report(QString::fromStdWString(diag.wstring()));CHECK(report.open(QIODevice::ReadOnly));auto object=QJsonDocument::fromJson(report.readAll()).object();
  CHECK(object.value("blocks_out").toInteger()>=8);CHECK(object.value("input_channels").toInt()==2 && object.value("output_channels").toInt()==3);
  CHECK(!object.contains("cinema_latency_samples"));CHECK(object.value("center_loaded").toBool() && object.value("lrc_loaded").toBool());
  if(attempt<2){CHECK(object.value("dynamic_recovery_loaded").toBool());CHECK(object.value("dynamic_recovery_active").toBool()==(attempt==1));}
  if(attempt==1){CHECK(object.value("dynamic_recovery_processed_blocks").toInteger()>0);std::cout<<"Recovery blocks="<<object.value("dynamic_recovery_processed_blocks").toInteger()<<" p95_us="<<object.value("dynamic_recovery_p95_us").toInteger()<<" max_us="<<object.value("dynamic_recovery_max_us").toDouble()<<"\n";}
  auto before=produced.load();Sleep(40);CHECK(produced.load()>before); // Source continues while Stage2 is stopped.
 }
 worker.request_stop();worker.join();source.stop();
 std::cout<<"PASS candidate OFF/ON, baseline v0010 OFF stereo L/R memcmp, package/controller/IPC and restart\n";
}
