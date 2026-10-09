#include "Engine/Input/Stage2Ingress.h"
#include "Engine/HighResolutionWait.h"
#include "Tests/Stage02/RealtimeAllocationAudit.h"
#include <StageLinkServer.h>
#include <StageBusValidation.h>
#include <windows.h>
#include <objbase.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>

using namespace entropy;
uint64_t tick(){LARGE_INTEGER q{};QueryPerformanceCounter(&q);return q.QuadPart;}
uint64_t frequency(){LARGE_INTEGER q{};QueryPerformanceFrequency(&q);return q.QuadPart;}
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Producer {
    link::StageLinkServer server;
    stage::StageBusWin32 bus;
    StageBusSessionHeader session{};
    std::array<uint8_t,16> lease{};
    std::atomic<bool> stop{false},starved{false},breakSession{false};
    std::atomic<unsigned> releases{0};
    std::thread thread;
    std::array<std::array<double,1024>,11> samples{};
    explicit Producer(std::wstring endpoint):server(std::move(endpoint)) {
        fresh();std::string error;require(server.start(error),error.c_str());
        thread=std::thread([this]{try{run();}catch(const std::exception& e){std::cerr<<"Fixture worker FAIL: "<<e.what()<<std::endl;stop=true;}});
    }
    ~Producer(){stop=true;thread.join();server.stop();bus.close();}
    void fresh(){GUID id{};CoCreateGuid(&id);session=stage::makeSession(LRC_CINEMA11_V1,reinterpret_cast<uint8_t*>(&id),frequency());session.producer_pid=GetCurrentProcessId();}
    void run(){
        control::HighResolutionWait wait;require(wait.valid(),"Fixture high-resolution timer unavailable");
        while(!stop){
            link::StageLinkRequest command{};uint32_t pid=0;
            if(server.tryTake(command,pid)){
                link::StageLinkResponse reply;reply.sourcePid=GetCurrentProcessId();std::memcpy(reply.nonce,command.nonce,16);
                if(command.action==link::StageLinkAction::Attach){
                    bus.close();fresh();std::string error;require(bus.create(session,error),error.c_str());GUID id{};CoCreateGuid(&id);std::memcpy(lease.data(),&id,16);
                }else if(command.action==link::StageLinkAction::Release){++releases;bus.close();fresh();}
                reply.status=link::StageLinkStatus::Ok;reply.session=session;reply.controlRevision=1;reply.program.required_inputs=3;reply.program.music_program_active=reply.program.analyze_music_final=1;std::memcpy(reply.lease,lease.data(),16);server.complete(reply);
            }
            bus.heartbeat(tick());if(breakSession.exchange(false)){bus.fault();bus.close();fresh();}
            StageBusFrameRequest request{};
            if(!starved&&bus.tryReceiveRequest(request)==stage::StageBusIoResult::Ok){
                StageBusBlockHeader header{};std::memcpy(header.session_id,request.session_id,16);header.stream_epoch=request.stream_epoch;header.block_sequence=request.block_sequence;header.first_frame=request.first_frame;header.control_revision=request.control_revision;header.frames=1024;header.validity_mask=7;header.qpc_timestamp=tick();
                std::array<const double*,11> pointers{};
                for(unsigned c=0;c<11;++c){pointers[c]=samples[c].data();for(unsigned f=0;f<1024;++f)samples[c][f]=f==0?-0.0:double((c+1)*100000+request.block_sequence*1024+f)/10000000.0;}
                require(bus.tryPublish(header,pointers.data())==stage::StageBusIoResult::Ok,"Fixture publish failed");
            }
            wait.forDuration(std::chrono::microseconds(100));
        }
    }
};
int main(int argc,char** argv) try {
    if(argc==3&&std::string(argv[1])=="--fixture-server"){
        std::string name=argv[2];Producer producer(std::wstring(name.begin(),name.end()));
        std::cout<<"Stage06 fixture producer READY"<<std::endl;Sleep(30000);return 0;
    }
    const auto endpoint=link::endpointName()+L".stage06-test."+std::to_wstring(GetCurrentProcessId());
    namespace audit=host::allocation_audit;require(audit::selfTest(),"RT audit positive control failed");audit::reset();
    auto input=std::make_unique<input::Stage2Ingress>();
    require(input->start(endpoint,false),"Input start failed");
    std::array<std::array<double,1024>,13> data{};std::array<double*,13> pointers{};for(unsigned c=0;c<13;++c)pointers[c]=data[c].data();
    auto readAudited=[&]{bool result=false;{audit::CycleScope scope;audit::audioPhase();result=input->tryRead(pointers.data());}return result;};
    require(!readAudited(),"Absent producer must not report signal");
    Producer producer(endpoint);
    const auto until=GetTickCount64()+6000;bool received=false;
    while(GetTickCount64()<until&&!received){received=readAudited();Sleep(1);}
    require(received,"Stage2 lease handoff must deliver real FP64 input");
    for(unsigned c=0;c<11;++c){require(std::signbit(data[c][0])&&data[c][0]==0,"Signed zero changed");for(unsigned f=1;f<1024;++f){double expected=double((c+1)*100000+1024+f)/10000000.0;require(std::memcmp(&expected,&data[c][f],8)==0,"FP64 lane/order/frame changed");}}
    for(unsigned c=11;c<13;++c)for(double value:data[c])require(value==0&&!std::signbit(value),"Reserved graph input must be positive zero");
    require(input->telemetry().producerPid==GetCurrentProcessId(),"Producer identity missing");
    // Catch default Windows Sleep granularity throttling the native receiver.
    // This is a transport throughput regression; DSP deadline gates stay separate.
    control::HighResolutionWait poll;require(poll.valid(),"Test timer unavailable");
    const auto throughputEnd=GetTickCount64()+3000;unsigned throughputBlocks=0;
    while(GetTickCount64()<throughputEnd){if(readAudited())++throughputBlocks;poll.forDuration(std::chrono::microseconds(100));}
    std::cout<<"Native ingress blocks in 3 seconds: "<<throughputBlocks<<std::endl;
    require(throughputBlocks>=500,"Native receiver throttled below sustained audio transport rate");
    producer.starved=true;Sleep(40);bool drained=false;for(unsigned i=0;i<8;++i)if(!readAudited())drained=true;
    require(drained,"Starvation must not replay old blocks");producer.starved=false;
    bool resumed=false;auto timeout=GetTickCount64()+2000;while(GetTickCount64()<timeout&&!resumed){resumed=readAudited();Sleep(1);}require(resumed,"Starvation recovery failed");
    const auto generation=input->telemetry().generation;producer.breakSession=true;
    timeout=GetTickCount64()+6000;bool recovered=false;while(GetTickCount64()<timeout&&!recovered){recovered=readAudited()&&input->telemetry().generation>generation;Sleep(1);}require(recovered,"Fresh session recovery failed");
    input->stop();require(producer.releases>0,"Clean stop did not release Stage2 lease");
    auto counts=audit::snapshot();audit::report();require(!counts.cppNew&&!counts.cppDelete&&!counts.heapAlloc&&!counts.heapFree&&!counts.wait&&!counts.lock&&!counts.io,"Ingress callback realtime safety failed");
    HANDLE owner=CreateMutexW(nullptr,FALSE,L"Local\\Entropy.Stage3.Render.Owner.Stage06.Test");require(owner&&GetLastError()!=ERROR_ALREADY_EXISTS,"Owner contention fixture could not acquire isolated render owner");
    auto contested=std::make_unique<input::Stage2Ingress>();require(contested->start(endpoint,true),"Contested ingress start failed");
    timeout=GetTickCount64()+2000;while(GetTickCount64()<timeout&&contested->telemetry().state!=input::State::OwnershipBusy)Sleep(1);
    require(contested->telemetry().state==input::State::OwnershipBusy,"Ownership contention was not reported");CloseHandle(owner);
    timeout=GetTickCount64()+6000;recovered=false;while(GetTickCount64()<timeout&&!recovered){recovered=contested->tryRead(pointers.data());Sleep(1);}require(recovered,"Ingress did not recover when old renderer released ownership");contested->stop();
    std::cout<<"Stage06 real StageBus lease, FP64 identity/order/-0, starvation, fresh-session recovery and release PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<"Stage06 FAIL: "<<error.what()<<'\n';return 1;}
