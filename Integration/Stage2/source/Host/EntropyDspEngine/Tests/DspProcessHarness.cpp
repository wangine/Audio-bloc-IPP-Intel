#include "TestSupport.h"
#include "DspSessionDiscovery.h"
#include "DspState.h"
#include "SourceCycleTimer.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <vector>
namespace fs=std::filesystem;
uint64_t pattern(uint64_t seq,unsigned c,unsigned i){if(i==0)return 0x8000000000000000ULL;if(i==1)return 1;if(i==2)return 0;return 0x3ff0000000000000ULL | ((seq*0x123457+c*0xabcdef+i*37)&0xfffffffffffffULL);}
void fill(Planes<10>& a,uint64_t seq){for(unsigned c=0;c<10;c++)for(unsigned i=0;i<1024;i++){auto x=pattern(seq,c,i);std::memcpy(&a.data[c][i],&x,8);}}
bool launch(const fs::path& exe,const std::wstring& args,PROCESS_INFORMATION& p){std::wstring command=L"\""+exe.wstring()+L"\" "+args;STARTUPINFOW start{};start.cb=sizeof(start);return CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&start,&p);}
bool waitChild(PROCESS_INFORMATION& p,DWORD& code){bool ok=WaitForSingleObject(p.hProcess,5000)==WAIT_OBJECT_0;if(ok)GetExitCodeProcess(p.hProcess,&code);else TerminateProcess(p.hProcess,99);CloseHandle(p.hThread);CloseHandle(p.hProcess);return ok;}
int main(int argc,char** argv){QCoreApplication app(argc,argv);std::string error;entropy::host::SourceCycleTimer timer;CHECK(timer.valid());
 if(argc>1 && std::string(argv[1])=="--recorder"){
  uint64_t token=std::stoull(argv[2]),blocks=std::stoull(argv[3]);fs::path root=argv[4];uint8_t uuid[16]{};std::memcpy(uuid,&token,8);uuid[15]=101;auto s=makeSession(2,uuid,frequency());StageBusWin32 bus;auto start=qpc();
  while(!bus.open(s,error)){CHECK(qpc()-start<frequency()*5);timer.waitUntil(std::chrono::steady_clock::now()+std::chrono::milliseconds(1));}
  CHECK(bus.address()!=reinterpret_cast<void*>(0x31000000000ULL));Planes<11> audio;std::vector<uint64_t> latency;latency.reserve(static_cast<size_t>(blocks));uint64_t maxQueue=0,controls=0,lastRevision=1,manual=0;auto epoch=std::chrono::steady_clock::now();auto begin=qpc();
  for(uint64_t seq=1;seq<=blocks;seq++){
   auto due=epoch+std::chrono::nanoseconds((seq-1)*1024ULL*1000000000ULL/192000);CHECK(timer.waitUntil(due));bus.heartbeat(qpc());
   StageBusSourceControl control{};auto ci=bus.tryReceiveSourceControl(control);CHECK(ci!=StageBusIoResult::Fault);if(ci==StageBusIoResult::Ok){controls++;lastRevision=std::max(lastRevision,control.source_revision);manual=control.manual_mute_mask;}
   auto r=request(s,seq);r.control_revision=lastRevision;r.program.manual_mute_mask=static_cast<uint32_t>(manual);r.program.effective_mute_mask=static_cast<uint32_t>(manual);auto sent=qpc();CHECK(bus.trySendRequest(r)==StageBusIoResult::Ok);
   StageBusBlockHeader b{};auto deadline=qpc()+frequency();StageBusIoResult result;
   do{result=bus.tryConsume(b,audio.out.data());if(result==StageBusIoResult::Empty)CHECK(timer.waitUntil(std::chrono::steady_clock::now()+std::chrono::microseconds(50)));}while(result==StageBusIoResult::Empty && qpc()<deadline);
   CHECK(result==StageBusIoResult::Ok);CHECK(b.block_sequence==seq && b.control_revision==r.control_revision && b.first_frame==(seq-1)*1024 && b.stream_epoch==1 && !std::memcmp(b.session_id,s.session_id,16));
   CHECK(b.source_generation[0]==seq/31+1 && b.source_generation[1]==seq/47+2 && b.source_generation[2]==seq/67+3 && b.validity_mask==7 && b.music_session_revision==23);
   for(unsigned c=0;c<10;c++)for(unsigned i=0;i<1024;i++)CHECK(bits(audio.data[c<2?c:c+1][i])==pattern(seq,c,i));
#ifdef CENTER_PROCESS_TEST
   for(unsigned i=0;i<1024;i++){double l,r;auto a=pattern(seq,0,i),b=pattern(seq,1,i);std::memcpy(&l,&a,8);std::memcpy(&r,&b,8);CHECK(bits(audio.data[2][i])==bits(.5*(l+r)));}
#else
   for(double x:audio.data[2])CHECK(bits(x)==0);
#endif

   latency.push_back(qpc()-sent);maxQueue=std::max(maxQueue,qpc()-b.qpc_timestamp);
  }
  auto elapsed=qpc()-begin;std::sort(latency.begin(),latency.end());auto us=[&](uint64_t q){return double(q)*1e6/frequency();};
  QJsonObject report{{"blocks",static_cast<qint64>(blocks)},{"frames",static_cast<qint64>(blocks*1024)},{"bitwise_values",static_cast<qint64>(blocks*10240)},{"elapsed_seconds",double(elapsed)/frequency()},{"payload_differences",0},{"duplicates",0},{"unexplained_gaps",0},{"faults",0},{"source_control_adoptions",static_cast<qint64>(controls)},{"roundtrip_p50_us",us(latency[latency.size()/2])},{"roundtrip_p95_us",us(latency[latency.size()*95/100])},{"roundtrip_p99_us",us(latency[latency.size()*99/100])},{"roundtrip_max_us",us(latency.back())},{"source_to_recorder_max_us",us(maxQueue)},{"scope","Synthetic conformity producer, DSP executable, recorder; no source provider or DAC"}};
  QFile file(QString::fromStdWString((root/"recorder.json").wstring()));CHECK(file.open(QIODevice::WriteOnly));CHECK(file.write(QJsonDocument(report).toJson())>0);file.close();std::ofstream(root/"done").put('1');
  auto waitStart=qpc();while(!fs::exists(root/"release")){CHECK(qpc()-waitStart<frequency()*5);bus.heartbeat(qpc());Sleep(10);}return 0;
 }
 unsigned seconds=argc>1?std::stoul(argv[1]):2;CHECK(seconds>0);QTemporaryDir temp;CHECK(temp.isValid());temp.setAutoRemove(false);fs::path root=temp.path().toStdWString();std::cout<<"EVIDENCE_ROOT "<<root<<std::endl;uint64_t token=qpc();uint8_t uuid[16]{};std::memcpy(uuid,&token,8);uuid[15]=101;auto s=makeSession(1,uuid,frequency());StageBusWin32 source;CHECK(source.create(s,error));CHECK(DspSessionDiscovery::write(root/"source.json",s,"EXTERNAL",error));
 wchar_t own[MAX_PATH];GetModuleFileNameW(nullptr,own,MAX_PATH);fs::path ownPath=own;auto engine=argc>3?fs::path(argv[3]):(fs::exists(ownPath.parent_path()/"package-manifest.json")?ownPath.parent_path()/"EntropyDspEngine.exe":ownPath.parent_path()/"DspTestEngine.exe");PROCESS_INFORMATION dsp{},recorder{};
#ifdef CENTER_PROCESS_TEST
 DspState state;state.centerConfigured=true;state.centerSettings.values[0]=0;CHECK(state.save(root/"state"/"Stage2State.json",error));const std::wstring centerOption=L" --center-native --lrc-native ";
#else
 const std::wstring centerOption;
#endif

 CHECK(launch(engine,centerOption+L"--headless --test-harness --runtime-root \""+root.wstring()+L"\" --source-session \""+(root/"source.json").wstring()+L"\" --stop-file \""+(root/"stop").wstring()+L"\" --diagnostic-file \""+(root/"dsp.json").wstring()+L"\"",dsp));
 uint64_t blocks=uint64_t(seconds)*192000/1024;CHECK(launch(ownPath,L"--recorder "+std::to_wstring(token)+L" "+std::to_wstring(blocks)+L" \""+root.wstring()+L"\"",recorder));
 StageBusSourceControl control{};std::memcpy(control.session_id,s.session_id,16);control.source_revision=1;control.music_right=1;CHECK(source.trySendSourceControl(control)==StageBusIoResult::Ok);
 Planes<10> audio;uint64_t count=0,heartbeat=0;auto parentBegin=qpc();
 while(!fs::exists(root/"done")){
  auto now=qpc();CHECK(now-parentBegin<frequency()*(seconds+15));CHECK(WaitForSingleObject(recorder.hProcess,0)==WAIT_TIMEOUT && WaitForSingleObject(dsp.hProcess,0)==WAIT_TIMEOUT);
  if(now-heartbeat>=frequency()/4){source.heartbeat(now);heartbeat=now;}
  StageBusFrameRequest r{};auto io=source.tryReceiveRequest(r);CHECK(io!=StageBusIoResult::Fault);
  if(io==StageBusIoResult::Ok){CHECK(r.block_sequence==++count);fill(audio,count);auto b=header(r);b.source_generation[0]=count/31+1;b.source_generation[1]=count/47+2;b.source_generation[2]=count/67+3;b.music_session_revision=23;CHECK(source.tryPublish(b,audio.in.data())==StageBusIoResult::Ok);
   if(count%187==0){control.source_revision++;control.manual_mute_mask=static_cast<uint32_t>(control.source_revision%8);auto sc=source.trySendSourceControl(control);CHECK(sc==StageBusIoResult::Ok || sc==StageBusIoResult::Full);}
  }else CHECK(timer.waitUntil(std::chrono::steady_clock::now()+std::chrono::microseconds(50)));
 }
 CHECK(count==blocks);std::ofstream(root/"stop").put('1');DWORD dspExit=99,recorderExit=99;CHECK(waitChild(dsp,dspExit));std::ofstream(root/"release").put('1');CHECK(waitChild(recorder,recorderExit));CHECK(dspExit==0 && recorderExit==0);
 if(argc>2){auto report=fs::path(argv[2]);fs::create_directories(report.parent_path());fs::copy_file(root/"recorder.json",report,fs::copy_options::overwrite_existing);fs::copy_file(root/"dsp.json",report.parent_path()/(report.stem().string()+"-dsp.json"),fs::copy_options::overwrite_existing);}
 std::cout<<"PASS 3-process conformity Source->DSP->recorder "<<blocks<<" blocks "<<blocks*1024<<" frames child exits "<<dspExit<<","<<recorderExit<<"; no hardware\n";
}
