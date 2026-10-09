#include "SourceController.h"
#include "StageLinkClient.h"
#include "TestSupport.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
using namespace entropy::host;
template<class Predicate> bool waitFor(Predicate p) {
    const auto until=GetTickCount64()+2000;
    while(!p()) { if(GetTickCount64()>until)return false; Sleep(1); }
    return true;
}
// Catches any reintroduction of downstream demand-owner takeover/session rotation.
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv); std::string error;
    if(argc==4 && std::string(argv[1])=="--subscriber-peer") {
        entropy::publish::Subscriber subscriber;
        if(!subscriber.connect(QString::fromLocal8Bit(argv[2]).toStdWString(),error))return 3;
        QFile ready(QString::fromLocal8Bit(argv[3]));if(!ready.open(QIODevice::WriteOnly))return 4;
        ready.write("ready");ready.close();
        for(;;){subscriber.heartbeat(qpc());Sleep(10);}
    }
    QTemporaryDir tmp;
    SourceController source;
    auto endpoint=entropy::link::endpointName()+L".passive-test."+std::to_wstring(GetCurrentProcessId());
    auto publication=entropy::publish::sourceEndpointName()+L".controller-test."+std::to_wstring(GetCurrentProcessId());
    CHECK(source.enablePublication(publication,error));
    CHECK(source.enableStageLink(endpoint,error));
    if(!source.start(TEST_MODULE_ROOT,std::filesystem::path(tmp.path().toStdWString()),error,true)) {
        std::cerr<<"Source startup: "<<error<<'\n';return 1;
    }
    CHECK(waitFor([&]{return source.blocks()>=8;}));
    const auto initial=source.snapshot();
    SourceCommand mode;mode.type=SourceCommand::BusMode;mode.value=1;
    CHECK(source.command(mode));Sleep(100);
    const auto afterMode=source.snapshot();
    CHECK(!afterMode.external);
    CHECK(!std::memcmp(initial.session.session_id,afterMode.session.session_id,16));
    CHECK(afterMode.blocks>initial.blocks && afterMode.faults==0);
    entropy::link::StageLinkClient client(endpoint);
    auto attach=entropy::link::makeRequest(entropy::link::StageLinkAction::Attach);
    std::memcpy(attach.expectedSession,initial.session.session_id,16);
    entropy::link::StageLinkResponse response{};
    CHECK(client.transact(attach,response,1000,error));
    CHECK(response.status==entropy::link::StageLinkStatus::Refused);
    CHECK(!std::memcmp(initial.session.session_id,response.session.session_id,16));
    auto release=entropy::link::makeRequest(entropy::link::StageLinkAction::Release);
    CHECK(client.transact(release,response,1000,error));
    CHECK(response.status==entropy::link::StageLinkStatus::Refused);

    // Catches paused cadence/session rotation for absent, closed or full V2 queues.
    const auto stable=[&](uint64_t before) {
        const auto s=source.snapshot();
        return s.blocks>before && s.faults==0 && !s.external && !s.capturing &&
            !std::memcmp(initial.session.session_id,s.session.session_id,16);
    };
    auto before=source.blocks();Sleep(100);CHECK(stable(before));
    entropy::publish::Subscriber healthy,slow;
    CHECK(healthy.connect(publication,error));
    CHECK(healthy.session().channels==10 && healthy.session().layout_id==MIX_CINEMA10_V1);
    CHECK(!std::memcmp(initial.session.session_id,healthy.session().session_id,16));
    Planes<11> planes;planes.data[10].fill(37.0);
    StageBusBlockHeader block{};
    auto receive=[&](entropy::publish::Subscriber& subscriber) {
        auto io=entropy::publish::ReadResult::Empty;
        return waitFor([&]{subscriber.heartbeat(qpc());io=subscriber.read(block,planes.out.data());
            return io!=entropy::publish::ReadResult::Empty;}) && io==entropy::publish::ReadResult::Block;
    };
    CHECK(receive(healthy));uint64_t previous=block.block_sequence;
    CHECK(block.first_frame==(block.block_sequence-1)*1024 && block.stream_epoch==1);
    CHECK(block.frames==1024 && block.validity_mask==0 && block.control_revision==initial.state.revision);
    CHECK(slow.connect(publication,error));
    before=source.blocks();const auto until=GetTickCount64()+150;unsigned received=0;
    while(GetTickCount64()<until) {
        healthy.heartbeat(qpc());slow.heartbeat(qpc());
        // Sleep(1) can span a Windows timer tick: drain every ready block.
        for(;;) {
            const auto io=healthy.read(block,planes.out.data());
            CHECK(io==entropy::publish::ReadResult::Empty || io==entropy::publish::ReadResult::Block);
            if(io==entropy::publish::ReadResult::Empty)break;
            CHECK(block.block_sequence==previous+1);previous=block.block_sequence;++received;
            CHECK(block.first_frame==(block.block_sequence-1)*1024 && block.stream_epoch==1);
            CHECK(!std::memcmp(initial.session.session_id,block.session_id,16));
            for(unsigned c=0;c<10;++c)for(const auto sample:planes.data[c])CHECK(bits(sample)==0);
            for(const auto sample:planes.data[10])CHECK(sample==37.0);
        }
        Sleep(1);
    }
    CHECK(received>=10 && stable(before));CHECK(source.snapshot().publishDrops>0);
    CHECK(slow.read(block,planes.out.data())==entropy::publish::ReadResult::Gap);
    slow.close();healthy.close();before=source.blocks();Sleep(100);CHECK(stable(before));
    // More than the transport's finite subscriber capacity: closed slots must retire.
    for(unsigned cycle=0;cycle<12;++cycle) {
        CHECK(healthy.connect(publication,error));CHECK(receive(healthy));
        CHECK(block.block_sequence>previous);previous=block.block_sequence;
        CHECK(!std::memcmp(initial.session.session_id,block.session_id,16));healthy.close();
    }
    before=source.blocks();source.mute(2);
    CHECK(waitFor([&]{return source.snapshot().state.manualMuteMask==2;}));
    CHECK(healthy.connect(publication,error));CHECK(receive(healthy));
    CHECK(block.control_revision>initial.state.revision);healthy.close();CHECK(stable(before));

    // A real subscriber process is terminated without close/destructors.
    const auto ready=std::filesystem::path(tmp.path().toStdWString())/"peer-ready";
    auto command=L"\""+QCoreApplication::applicationFilePath().toStdWString()+L"\" --subscriber-peer \""+publication+L"\" \""+ready.wstring()+L"\"";
    struct Peer {
        PROCESS_INFORMATION info{};
        ~Peer(){if(info.hProcess){if(WaitForSingleObject(info.hProcess,0)==WAIT_TIMEOUT)TerminateProcess(info.hProcess,99);CloseHandle(info.hProcess);}if(info.hThread)CloseHandle(info.hThread);}
    } peer;
    STARTUPINFOW startup{};startup.cb=sizeof(startup);
    CHECK(CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&peer.info));
    CHECK(waitFor([&]{return std::filesystem::exists(ready);}));
    before=source.blocks();Sleep(100);CHECK(stable(before));
    CHECK(TerminateProcess(peer.info.hProcess,99));
    CHECK(WaitForSingleObject(peer.info.hProcess,2000)==WAIT_OBJECT_0);
    before=source.blocks();Sleep(100);CHECK(stable(before));
    CHECK(healthy.connect(publication,error));CHECK(receive(healthy));healthy.close();
    CHECK(!std::memcmp(initial.session.session_id,block.session_id,16));
    std::cout<<"PASS real offline controller: absent/full/closed/crashed/reconnecting subscribers; blocks="<<source.blocks()
        <<" faults="<<source.snapshot().faults<<" subscriberDrops="<<source.snapshot().publishDrops<<" fixed UUID, exact FP64 zero and untouched eleventh plane\n";
    source.stop();
}
