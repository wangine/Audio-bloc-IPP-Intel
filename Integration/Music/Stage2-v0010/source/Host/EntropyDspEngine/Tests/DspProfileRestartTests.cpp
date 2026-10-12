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
struct ProfileChild {
 PROCESS_INFORMATION p{};
 ~ProfileChild(){if(p.hProcess){if(WaitForSingleObject(p.hProcess,0)==WAIT_TIMEOUT){TerminateProcess(p.hProcess,91);WaitForSingleObject(p.hProcess,2000);}CloseHandle(p.hThread);CloseHandle(p.hProcess);}}
 bool start(const std::wstring& exe,std::wstring args){STARTUPINFOW info{};info.cb=sizeof(info);auto command=L"\""+exe+L"\" "+args;return CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&info,&p);}
};
int main(int argc,char** argv){
 QCoreApplication app(argc,argv);CHECK(argc==2);auto executable=std::filesystem::path(argv[1]).wstring();QTemporaryDir tmp;CHECK(tmp.isValid());auto root=std::filesystem::path(tmp.path().toStdWString());std::string error;
 const auto profile=root/std::filesystem::path(L"Profil pentru restart.Edsp");DspState cached;cached.centerConfigured=true;cached.centerSettings.values[0]=1;cached.alignmentActive=true;
 CHECK(cached.save(profile,error));CHECK(cached.remember(root,QString::fromStdWString(profile.wstring()),error));DspState external=cached;external.centerSettings.values[0]=0;CHECK(external.save(profile,error));
 auto endpoint=musicEndpoint(L".profile-restart");auto s=session(3);s.producer_pid=GetCurrentProcessId();entropy::publish::Publisher source;CHECK(source.start(endpoint,error));source.setSession(s);std::atomic<uint64_t> produced{0};
 std::jthread worker([&](std::stop_token stop){Planes<2> planes;planes.data[0].fill(.1);planes.data[1].fill(.075);uint64_t seq=71;while(!stop.stop_requested()){source.service();source.publish(musicHeader(s,seq++),planes.in.data());++produced;std::this_thread::sleep_for(std::chrono::milliseconds(6));}});
 for(unsigned attempt=0;attempt<2;++attempt){
  if(attempt){QFile selected(QString::fromStdWString(profile.wstring()));CHECK(selected.remove());}
  const auto stop=root/("stop-"+std::to_string(attempt)),diag=root/("diag-"+std::to_string(attempt)+".json");
  auto args=L"--headless --automatic --runtime-root \""+root.wstring()+L"\" --link-endpoint \""+endpoint+L"\" --stop-file \""+stop.wstring()+L"\" --diagnostic-file \""+diag.wstring()+L"\"";
  ProfileChild child;CHECK(child.start(executable,args));entropy::publish::Subscriber output;bool connected=false;auto deadline=qpc()+frequency()*8;
  while(qpc()<deadline&&WaitForSingleObject(child.p.hProcess,0)==WAIT_TIMEOUT){if(output.connect(endpoint+L".StagePublish.V2",error)){connected=true;break;}Sleep(20);}CHECK(connected);
  Planes<3> planes;StageBusBlockHeader header{};unsigned blocks=0;bool restored=false;deadline=qpc()+frequency()*8;
  while(qpc()<deadline){output.heartbeat(qpc());auto result=output.read(header,planes.out.data());if(result==entropy::publish::ReadResult::Block)++blocks;else if(result!=entropy::publish::ReadResult::Empty){Sleep(1500);QFile report(QString::fromStdWString(diag.wstring()));if(report.open(QIODevice::ReadOnly))std::cerr<<report.readAll().toStdString();DWORD code=0;GetExitCodeProcess(child.p.hProcess,&code);std::cerr<<"\nRead result="<<int(result)<<" attempt="<<attempt<<" blocks="<<blocks<<" childExit="<<code<<" sourceDrops="<<source.dropped()<<"\n";CHECK(result==entropy::publish::ReadResult::Empty);}
   // Release the reader before waiting: Windows rejects QSaveFile replacement while a reader holds the old file.
   {QFile report(QString::fromStdWString(diag.wstring()));if(report.open(QIODevice::ReadOnly)){auto object=QJsonDocument::fromJson(report.readAll()).object();const auto latency=object.value("music_cx_latency_samples");restored=object.value("lrc_alignment_active").toBool()&&latency.isDouble()&&(attempt?latency.toInt()>0:latency.toInt()==0);}}
   if(blocks>=16&&restored)break;Sleep(2);
  }
  CHECK(blocks>=16&&restored);output.close();std::ofstream(stop).put('1');CHECK(WaitForSingleObject(child.p.hProcess,4000)==WAIT_OBJECT_0);DWORD code=99;CHECK(GetExitCodeProcess(child.p.hProcess,&code)&&code==0);auto before=produced.load();Sleep(30);CHECK(produced.load()>before);
 }
 worker.request_stop();worker.join();source.stop();std::cout<<"PASS packaged restart uses selected profile, missing file falls back to valid cache, LRC restored via existing APPLY, isolated source remains independent\n";
}
