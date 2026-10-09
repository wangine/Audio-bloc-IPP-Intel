#include "StagePublishV2.h"
#include "StageBusValidation.h"
#include "StagePublishV2Protocol.h"
#include "StageLinkSecurity.h"
#include <windows.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace entropy::publish;
void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
using Audio=std::array<std::array<double,1024>,11>;
struct Fixture {
    Publisher publisher;
    StageBusSessionHeader session{};
    std::wstring endpoint;
    Audio input{},output{};
    std::array<const double*,11> in{};
    std::array<double*,11> out{};
    Fixture() {
        LARGE_INTEGER qpc{};QueryPerformanceFrequency(&qpc);
        const uint8_t uuid[16]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
        session=entropy::stage::makeSession(LRC_CINEMA11_V1,uuid,qpc.QuadPart);
        session.producer_pid=GetCurrentProcessId();
        endpoint=endpointName()+L".selftest."+std::to_wstring(GetCurrentProcessId());
        for(size_t c=0;c<11;c++){in[c]=input[c].data();out[c]=output[c].data();for(size_t i=0;i<1024;i++)input[c][i]=double(c*1024+i)/17.;}
        std::string error;check(publisher.start(endpoint,error),error.c_str());publisher.setSession(session);
    }
    StageBusBlockHeader block(uint64_t sequence) {
        StageBusBlockHeader b{};std::memcpy(b.session_id,session.session_id,16);
        b.stream_epoch=55;b.block_sequence=sequence;b.first_frame=700000+(sequence-1)*1024;
        b.control_revision=876;b.qpc_timestamp=9876543+sequence;b.music_session_revision=123;
        b.frames=1024;b.validity_mask=7;b.source_generation[0]=67;b.source_rate[0]=48000;return b;
    }
    bool connect(Subscriber& subscriber) {
        std::atomic<bool> done{false};bool ok=false;std::string error;
        std::thread client([&]{ok=subscriber.connect(endpoint,error);done=true;});
        while(!done){publisher.service();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        client.join();if(!ok)std::cerr<<"connect: "<<error<<'\n';return ok;
    }
};
struct Child {
    PROCESS_INFORMATION process{};HANDLE ready=nullptr;std::wstring event;
    Child(const std::wstring& mode,const std::wstring& endpoint=L"") {
        LARGE_INTEGER qpc{};QueryPerformanceCounter(&qpc);
        event=L"Local\\StagePublishV2Test."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(qpc.QuadPart);
        ready=CreateEventW(nullptr,TRUE,FALSE,event.c_str());check(ready!=nullptr,"child readiness event");
        wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
        // Optional different-compiler binary exercises the same wire across MSVC/MinGW.
        wchar_t alternate[32768]{};
        const auto alternateLength=GetEnvironmentVariableW(L"STAGE_PUBLISH_V2_CHILD_EXE",alternate,32768);
        if(alternateLength && alternateLength<32768)std::memcpy(executable,alternate,(alternateLength+1)*sizeof(wchar_t));
        std::wstring command=L"\""+std::wstring(executable)+L"\" "+mode+L" "+event;
        if(!endpoint.empty())command+=L" "+endpoint;
        STARTUPINFOW startup{};startup.cb=sizeof(startup);
        check(CreateProcessW(executable,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)!=FALSE,"launch isolated transport child");
    }
    void kill() {if(process.hProcess){TerminateProcess(process.hProcess,73);check(WaitForSingleObject(process.hProcess,2000)==WAIT_OBJECT_0,"transport child terminated");}}
    ~Child(){if(process.hProcess){TerminateProcess(process.hProcess,73);WaitForSingleObject(process.hProcess,2000);CloseHandle(process.hProcess);CloseHandle(process.hThread);}if(ready)CloseHandle(ready);}
};
int main(int argc,char** argv) {
    if(argc>=3){
        auto wide=[](const char* p){return std::wstring(p,p+std::strlen(p));};
        HANDLE ready=OpenEventW(EVENT_MODIFY_STATE,FALSE,wide(argv[2]).c_str());if(!ready)return 8;
        if(std::strcmp(argv[1],"--child-publisher")==0){Fixture f;SetEvent(ready);CloseHandle(ready);uint64_t sequence=50000;for(;;){f.publisher.service();f.publisher.publish(f.block(sequence++),f.in.data());Sleep(5);}}
        if(std::strcmp(argv[1],"--child-subscriber")==0 && argc==4){Subscriber s;std::string error;if(!s.connect(wide(argv[3]),error))return 9;SetEvent(ready);CloseHandle(ready);Sleep(INFINITE);}
        CloseHandle(ready);return 10;
    }
    try {
        Fixture f; Subscriber s;check(f.connect(s),"connect must create passive subscription");
        check(!std::memcmp(&s.session(),&f.session,sizeof(f.session)),"original session metadata preserved");
        StageBusBlockHeader received{};check(s.read(received,f.out.data())==ReadResult::Empty,"fresh queue empty");
        auto b=f.block(9001);f.publisher.publish(b,f.in.data());
        check(s.read(received,f.out.data())==ReadResult::Block,"first source sequence establishes cursor");
        check(!std::memcmp(&b,&received,sizeof(b)),"all source metadata preserved");
        check(!std::memcmp(f.input.data(),f.output.data(),sizeof(Audio)),"binary64 planes exact");
        std::cout<<"PASS exact payload and metadata\n";
        s.close();f.publisher.service();f.publisher.publish(f.block(9002),f.in.data());
        check(f.publisher.dropped()==0,"absent consumer does not affect producer");
        check(f.connect(s),"fresh reconnect");check(s.read(received,f.out.data())==ReadResult::Empty,"no backlog on reconnect");
        f.publisher.publish(f.block(10000),f.in.data());check(s.read(received,f.out.data())==ReadResult::Block,"reconnect keeps real sequence");
        f.publisher.publish(f.block(10002),f.in.data());check(s.read(received,f.out.data())==ReadResult::Gap,"sequence gap explicit");
        check(s.read(received,f.out.data())==ReadResult::Disconnected,"gap closes only local subscription");
        check(f.connect(s),"subscribe after gap");
        for(uint64_t i=0;i<40;i++)f.publisher.publish(f.block(20000+i),f.in.data());
        check(f.publisher.dropped()>0,"full subscription drops delivery");
        check(s.read(received,f.out.data())==ReadResult::Gap,"full queue never replays stale backlog");
        std::cout<<"PASS gap, full, absent consumer and fresh reconnect\n";
        check(f.connect(s),"subscribe before invalid header");
        b=f.block(30000);b.reserved=1;f.publisher.publish(b,f.in.data());
        check(s.read(received,f.out.data())==ReadResult::Invalid,"malformed source block fails locally");
        check(f.connect(s),"subscribe before source discontinuity");
        b=f.block(31000);b.discontinuity_mask=1;f.publisher.publish(b,f.in.data());
        check(s.read(received,f.out.data())==ReadResult::Block,"cumulative source flag does not invent delivery gap");
        check(!std::memcmp(&b,&received,sizeof(b)),"source discontinuity flag preserved exactly");
        b=f.block(31001);b.discontinuity_mask=1;f.publisher.publish(b,f.in.data());
        check(s.read(received,f.out.data())==ReadResult::Block,"continuous source blocks with repeated telemetry flag delivered");
        check(!std::memcmp(&b,&received,sizeof(b)),"repeated source flag remains visible without delivery loss");
        s.close();f.publisher.service();
        check(f.connect(s),"subscribe before epoch change");b=f.block(32000);f.publisher.publish(b,f.in.data());
        check(s.read(received,f.out.data())==ReadResult::Block,"epoch cursor established");
        b=f.block(32001);b.stream_epoch++;f.publisher.publish(b,f.in.data());
        check(s.read(received,f.out.data())==ReadResult::Gap,"epoch change explicit");
        check(f.connect(s),"subscribe before first-frame gap");b=f.block(33000);f.publisher.publish(b,f.in.data());
        check(s.read(received,f.out.data())==ReadResult::Block,"frame cursor established");
        b=f.block(33001);b.first_frame++;f.publisher.publish(b,f.in.data());
        check(s.read(received,f.out.data())==ReadResult::Gap,"first-frame gap explicit");
        check(f.connect(s),"subscribe before heartbeat expiration");s.heartbeat(1);f.publisher.service();
        check(s.read(received,f.out.data())==ReadResult::Disconnected,"expired heartbeat closes local queue");
        std::cout<<"PASS invalid header, discontinuity, epoch, frame and heartbeat\n";
        std::array<Subscriber,4> independent;for(auto& subscriber:independent)check(f.connect(subscriber),"four independent subscribers");
        Subscriber fifth;check(!f.connect(fifth),"fifth subscriber refused without disturbing existing queues");
        const auto dropsBefore=f.publisher.dropped();
        for(uint64_t i=0;i<40;i++){
            f.publisher.publish(f.block(40000+i),f.in.data());
            for(size_t c=1;c<4;c++)check(independent[c].read(received,f.out.data())==ReadResult::Block,"slow subscriber cannot stall active subscribers");
        }
        check(f.publisher.dropped()>dropsBefore,"slow subscription delivery drops counted");
        check(independent[0].read(received,f.out.data())==ReadResult::Gap,"only slow subscriber gaps");
        for(auto& subscriber:independent)subscriber.close();f.publisher.service();
        std::cout<<"PASS bounded capacity and independent subscribers\n";
        for(int malformed=0;malformed<4;malformed++) {
            entropy::publish::wire::Request request{};request.clientPid=GetCurrentProcessId();request.nonce[0]=1;
            if(malformed==0)request.clientPid++;if(malformed==1)request.magic++;if(malformed==2)request.bytes--;if(malformed==3)request.reserved=1;
            HANDLE pipe=CreateFileW(f.endpoint.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
            if(pipe==INVALID_HANDLE_VALUE && WaitNamedPipeW(f.endpoint.c_str(),1000))pipe=CreateFileW(f.endpoint.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
            check(pipe!=INVALID_HANDLE_VALUE,"open malformed request fixture");DWORD mode=PIPE_READMODE_MESSAGE;SetNamedPipeHandleState(pipe,&mode,nullptr,nullptr);
            check(entropy::link::transfer(pipe,true,&request,sizeof(request),1000),"write malformed request");
            entropy::publish::wire::Response response{};check(!entropy::link::transfer(pipe,false,&response,sizeof(response),1000),"forged PID/malformed wire rejected");CloseHandle(pipe);
        }
        std::cout<<"PASS authenticated client PID and malformed wire rejection\n";
        {
            Child child(L"--child-subscriber",f.endpoint);
            const auto deadline=GetTickCount64()+5000;
            while(WaitForSingleObject(child.ready,0)!=WAIT_OBJECT_0 && GetTickCount64()<deadline){f.publisher.service();Sleep(1);}
            check(WaitForSingleObject(child.ready,0)==WAIT_OBJECT_0,"crash fixture subscriber connected");
            child.kill();f.publisher.service();
            std::array<Subscriber,4> recovered;for(auto& subscriber:recovered)check(f.connect(subscriber),"crashed subscriber frees only its slot");
            for(auto& subscriber:recovered)subscriber.close();f.publisher.service();
        }
        {
            Child child(L"--child-publisher");check(WaitForSingleObject(child.ready,5000)==WAIT_OBJECT_0,"crash fixture publisher ready");
            Subscriber local;std::string error;
            check(local.connect(endpointName()+L".selftest."+std::to_wstring(child.process.dwProcessId),error),"cross-process publisher subscription");
            auto result=ReadResult::Empty;const auto deadline=GetTickCount64()+1000;
            while(result==ReadResult::Empty && GetTickCount64()<deadline){result=local.read(received,f.out.data());if(result==ReadResult::Empty)Sleep(1);}
            check(result==ReadResult::Block,"cross-process block delivered");
            b=f.block(received.block_sequence);check(!std::memcmp(&b,&received,sizeof(b)),"cross-process source metadata preserved");
            check(!std::memcmp(f.input.data(),f.output.data(),sizeof(Audio)),"cross-process binary64 payload exact");
            child.kill();LARGE_INTEGER qpc{};QueryPerformanceCounter(&qpc);local.heartbeat(qpc.QuadPart);
            check(local.read(received,f.out.data())==ReadResult::Disconnected,"publisher crash detected by control heartbeat");
        }
        std::cout<<"PASS cross-process subscriber and publisher crash isolation\n";
        check(f.connect(s),"connect before stop");f.publisher.stop();
        check(s.read(received,f.out.data())==ReadResult::Disconnected,"publisher stop is local disconnect");
        std::cout<<"PASS producer stop liveness\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
