#include "SourceController.h"
#include "TestSupport.h"
#include <QCoreApplication>
#include <QTemporaryDir>
using namespace entropy::host;
template<class P>static bool waitFor(P p){auto until=GetTickCount64()+2000;while(!p()){if(GetTickCount64()>until)return false;Sleep(1);}return true;}
namespace entropy::host {struct SourceLinkRaceProbe {
 static bool selectSynthetic(SourceController& c,std::string& error){std::lock_guard lock(c.mutex_);c.offline_=false;c.capturing_=true;c.catalog_.entries.clear();InputSourceEntry entry;entry.key=SourceStartupPolicy::musicKey();entry.selectable=true;c.catalog_.entries.push_back(entry);c.selector_=std::make_unique<SourceSelectionController>(c.domains_,TEST_SELECTION_FIXTURE_ROOT);c.selector_->engine=0;SourceCommand select;select.type=SourceCommand::Select;select.key=entry.key;select.role=0;select.value=1;c.service(select);return c.domains_.musicPrepared();}
};}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir tmp;std::string error;SourceController c;CHECK(c.start(TEST_MODULE_ROOT,std::filesystem::path(tmp.path().toStdWString()),error,true));SourceCommand mode;mode.type=SourceCommand::BusMode;mode.value=1;CHECK(c.command(mode));CHECK(waitFor([&]{return c.snapshot().external;}));auto initial=c.snapshot();StageBusWin32 peer;CHECK(peer.open(initial.session,error));peer.heartbeat(qpc());for(unsigned seq=1;seq<=5;++seq){auto req=request(initial.session,seq);CHECK(peer.trySendSourceRequest(req)==StageBusIoResult::Ok);if(seq<5)CHECK(waitFor([&]{return c.snapshot().sequence==seq;}));}Sleep(30);
CHECK(SourceLinkRaceProbe::selectSynthetic(c,error));auto selected=c.snapshot();CHECK(selected.state.revision>initial.state.revision);CHECK(!std::memcmp(selected.session.session_id,initial.session.session_id,16));CHECK(selected.sequence==4&&selected.faults==0);CHECK(waitFor([&]{return c.snapshot().sources[0].fifo.fill_frames>=8192;}));
Planes<10> audio;StageBusBlockHeader block;for(unsigned seq=1;seq<=5;++seq){StageBusIoResult io=StageBusIoResult::Empty;CHECK(waitFor([&]{peer.heartbeat(qpc());io=peer.tryConsume(block,audio.out.data());return io!=StageBusIoResult::Empty;}));CHECK(io==StageBusIoResult::Ok&&block.block_sequence==seq);if(seq==5){CHECK(block.first_frame==4096);CHECK(block.validity_mask&1);CHECK(block.discontinuity_mask&1);}}
CHECK(c.snapshot().sequence==5&&c.snapshot().faults==0);c.stop();std::cout<<"PASS live synthetic source Select while request5 pending: same UUID, contiguous delivery, MUSIC validity, local discontinuity, no transport loss\n";}
