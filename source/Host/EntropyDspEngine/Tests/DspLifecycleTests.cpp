#include "TestSupport.h"
#include "DspController.h"
#include "DspSessionDiscovery.h"
#include <QCoreApplication>
#include <QTemporaryDir>
namespace entropy::stage {struct DiscoveryRaceProbe {std::filesystem::path path;StageBusSessionHeader session;static void replace(void* p){auto& probe=*static_cast<DiscoveryRaceProbe*>(p);std::string e;DspSessionDiscovery::write(probe.path,probe.session,"STANDALONE",e);}void arm(DspController& c){c.attachmentProbe_=replace;c.probeContext_=this;}};}
int main(int argc,char** argv){QCoreApplication app(argc,argv);QTemporaryDir dir;auto root=std::filesystem::path(dir.path().toStdWString());auto s=session(1);StageBusWin32 source;std::string error;
 CHECK(source.create(s,error));CHECK(DspSessionDiscovery::write(root/"source.json",s,"STANDALONE",error));StageBusSessionHeader parsed{};CHECK(!DspSessionDiscovery::readExternal(root/"source.json",parsed,error));
 CHECK(DspSessionDiscovery::write(root/"source.json",s,"EXTERNAL",error));CHECK(DspSessionDiscovery::readExternal(root/"source.json",parsed,error));CHECK(!std::memcmp(&s,&parsed,256));
 {DspController race;DiscoveryRaceProbe probe{root/"source.json",s};probe.arm(race);CHECK(!race.start(root,root/"source.json",error));CHECK(!std::filesystem::exists(root/"state"/"StageBusSession.json"));}
 source.close();s=session(1);CHECK(source.create(s,error));CHECK(DspSessionDiscovery::write(root/"source.json",s,"EXTERNAL",error));
 DspController controller;CHECK(controller.start(root,root/"source.json",error));CHECK(!controller.start(root,root/"source.json",error));controller.stop();CHECK(!controller.start(root,root/"source.json",error)); // claimed/faulted old session cannot be resumed
 source.close();s=session(1);CHECK(source.create(s,error));CHECK(DspSessionDiscovery::write(root/"source.json",s,"EXTERNAL",error));CHECK(controller.start(root,root/"source.json",error));source.fault();Sleep(350);CHECK(controller.snapshot().state==DspStateCode::Fault);controller.stop();
 source.close();s=session(1);CHECK(source.create(s,error));CHECK(DspSessionDiscovery::write(root/"source.json",s,"EXTERNAL",error));CHECK(controller.start(root,root/"source.json",error));auto deadline=qpc()+frequency()*6;while(qpc()<deadline && controller.snapshot().state!=DspStateCode::Fault){source.heartbeat(qpc());Sleep(20);}CHECK(controller.snapshot().state==DspStateCode::Fault);CHECK(std::strstr(controller.snapshot().error,"handshake timeout"));controller.stop();
 std::cout<<"PASS standalone rejected, exact discovery, repeated start refused, old UUID rejected, fresh UUID recovery and peer fault\n";
}
