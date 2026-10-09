#include "Engine/Input/Stage2Ingress.h"
#include "Engine/HighResolutionWait.h"
#include "Tests/Stage02/RealtimeAllocationAudit.h"
#include <StageBusValidation.h>
#include <windows.h>
#include <objbase.h>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace entropy;
uint64_t tick(){LARGE_INTEGER q{};QueryPerformanceCounter(&q);return q.QuadPart;}
uint64_t frequency(){LARGE_INTEGER q{};QueryPerformanceFrequency(&q);return q.QuadPart;}
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
// Nominal publication continues independently of consumers.
struct Producer {
 publish::Publisher publisher;StageBusSessionHeader session{};
 std::atomic<bool> stop{false},starved{false},nonFiniteBlock{false};std::atomic<uint64_t> blocks{0};std::thread thread;
 std::array<std::array<double,1024>,11> samples{};
 explicit Producer(const std::wstring& endpoint){
  GUID id{};CoCreateGuid(&id);session=stage::makeSession(LRC_CINEMA11_V1,reinterpret_cast<uint8_t*>(&id),frequency());session.producer_pid=GetCurrentProcessId();
  std::string error;require(publisher.start(endpoint,error),error.c_str());publisher.setSession(session);
  thread=std::thread([this]{control::HighResolutionWait wait;auto next=std::chrono::steady_clock::now();uint64_t sequence=0;
   while(!stop){publisher.service();const auto now=std::chrono::steady_clock::now();
    if(!starved&&now>=next){StageBusBlockHeader h{};std::memcpy(h.session_id,session.session_id,16);h.stream_epoch=1;h.block_sequence=++sequence;h.first_frame=(sequence-1)*1024;h.control_revision=1;h.frames=1024;h.validity_mask=7;h.qpc_timestamp=tick();
     std::array<const double*,11> planes{};for(unsigned c=0;c<11;++c){planes[c]=samples[c].data();for(unsigned f=0;f<1024;++f)samples[c][f]=f==0?-0.0:double((c+1)*100000+sequence*1024+f)/10000000.;}
     if(nonFiniteBlock.exchange(false))samples[0][1]=std::numeric_limits<double>::quiet_NaN();
     publisher.publish(h,planes.data());blocks=sequence;next=now+std::chrono::nanoseconds(5333333);
    }wait.forDuration(std::chrono::microseconds(100));
   }});
 }
 ~Producer(){stop=true;thread.join();publisher.stop();}
};
int main(int argc,char** argv) try {
 if(argc==3&&std::string(argv[1])=="--fixture-server"){
  std::string name=argv[2];Producer producer(std::wstring(name.begin(),name.end()));std::cout<<"Stage06 V2 passive fixture READY"<<std::endl;Sleep(30000);return 0;
 }
 const auto endpoint=publish::endpointName()+L".stage06-test."+std::to_wstring(GetCurrentProcessId());
 namespace audit=host::allocation_audit;require(audit::selfTest(),"RT audit positive control failed");audit::reset();
 auto input=std::make_unique<input::Stage2Ingress>();require(input->start(endpoint,false),"Input start failed");
 std::array<std::array<double,1024>,13> data{};std::array<double*,13> planes{};for(unsigned c=0;c<13;++c)planes[c]=data[c].data();
 auto read=[&]{audit::CycleScope scope;audit::audioPhase();return input->tryRead(planes.data());};
 auto awaitRead=[&]{const auto end=GetTickCount64()+6000;while(GetTickCount64()<end){if(read())return true;Sleep(1);}return false;};
 require(!read(),"Absent publisher fabricated signal");Producer producer(endpoint);require(awaitRead(),"Passive input failed");const auto sequence=input->telemetry().lastSequence;
 for(unsigned c=0;c<11;++c){require(std::signbit(data[c][0])&&data[c][0]==0,"Signed zero changed");for(unsigned f=1;f<1024;++f){double expected=double((c+1)*100000+sequence*1024+f)/10000000.;require(std::memcmp(&expected,&data[c][f],8)==0,"FP64 lane/frame changed");}}
 for(unsigned c=11;c<13;++c)for(double value:data[c])require(value==0&&!std::signbit(value),"Reserved graph input changed");
 require(input->telemetry().producerPid==GetCurrentProcessId(),"Missing identity");require(std::memcmp(input->telemetry().sourceSession.data(),producer.session.session_id,16)==0,"Source UUID changed");
 control::HighResolutionWait poll;const auto end=GetTickCount64()+3000;unsigned count=0;
 while(GetTickCount64()<end){if(read())++count;poll.forDuration(std::chrono::microseconds(100));}
 std::cout<<"Ingress blocks/3s="<<count<<'\n';require(count>=500,"Receiver throttled nominal publication");
 producer.starved=true;Sleep(20);while(read()){}require(!read(),"Starvation replayed old audio");producer.starved=false;require(awaitRead(),"Starvation resume failed");
 const auto before=producer.blocks.load();Sleep(180);require(!read(),"Overflow replayed stale audio");require(input->telemetry().gaps>0&&input->telemetry().state==input::State::Fault,"Overflow must be explicit local gap");require(producer.blocks>before+20,"Stopped consumer throttled publisher");
 input->restartSubscription();require(awaitRead(),"Explicit fresh subscription failed");require(input->telemetry().lastSequence>before,"Fresh subscription replayed backlog");
 producer.nonFiniteBlock=true;const auto deadline=GetTickCount64()+6000;while(GetTickCount64()<deadline&&input->telemetry().state!=input::State::Fault){read();Sleep(1);}
 require(input->telemetry().state==input::State::Fault,"NaN not rejected locally");const auto faultSequence=producer.blocks.load();Sleep(30);require(producer.blocks>faultSequence,"NaN local rejection faulted publisher");input->restartSubscription();require(awaitRead(),"NaN fresh subscription failed");
 for(unsigned attempt=0;attempt<8;++attempt){input->stop();const auto cursor=producer.blocks.load();Sleep(30);require(producer.blocks>cursor,"Consumer stop stalled source");require(input->start(endpoint,false),"Input restart failed");require(awaitRead(),"Restart lost healthy publisher");require(std::memcmp(input->telemetry().sourceSession.data(),producer.session.session_id,16)==0,"Restart replaced source UUID");}
 input->stop();auto counts=audit::snapshot();audit::report();require(!counts.cppNew&&!counts.cppDelete&&!counts.heapAlloc&&!counts.heapFree&&!counts.wait&&!counts.lock&&!counts.io,"Ingress callback RT safety failed");
 HANDLE owner=CreateMutexW(nullptr,FALSE,L"Local\\Entropy.Stage3.Render.Owner.Stage06.Test");require(owner&&GetLastError()!=ERROR_ALREADY_EXISTS,"Owner fixture failed");auto contested=std::make_unique<input::Stage2Ingress>();require(contested->start(endpoint,true),"Contested ingress failed");auto timeout=GetTickCount64()+2000;while(GetTickCount64()<timeout&&contested->telemetry().state!=input::State::OwnershipBusy)Sleep(1);require(contested->telemetry().state==input::State::OwnershipBusy,"Local owner contention not reported");CloseHandle(owner);
 timeout=GetTickCount64()+6000;bool recovered=false;while(GetTickCount64()<timeout&&!recovered){recovered=contested->tryRead(planes.data());Sleep(1);}require(recovered,"Local owner recovery failed");contested->stop();
 std::cout<<"PASS passive FP64/order/-0, starvation, gap, NaN, eight restarts, unchanged source UUID, RT audit\n";return 0;
}catch(const std::exception& e){std::cerr<<"Stage06 FAIL: "<<e.what()<<'\n';return 1;}
