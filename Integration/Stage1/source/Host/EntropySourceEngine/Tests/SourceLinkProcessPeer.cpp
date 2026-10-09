#include "SourceController.h"
#include <QCoreApplication>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
#include <fstream>
namespace entropy::host {struct SourceLinkRaceProbe {
 std::atomic<bool> ready{false},release{false},done{false};
 static void hold(void* opaque){auto& probe=*static_cast<SourceLinkRaceProbe*>(opaque);if(probe.done.exchange(true))return;probe.ready=true;while(!probe.release.load())Sleep(1);}
 void arm(SourceController& c,bool grant){c.linkProbeContext_=this;if(grant)c.leaseGrantedProbe_=hold;else c.requestAdoptedProbe_=hold;}
};}
using namespace entropy::host;
int main(int argc,char** argv){QCoreApplication app(argc,argv);if(argc!=4)return 2;std::filesystem::path root=QString::fromLocal8Bit(argv[1]).toStdWString();auto endpoint=QString::fromLocal8Bit(argv[2]).toStdWString();SourceController controller;std::string error;
 SourceLinkRaceProbe probe;const std::string mode=argv[3];bool paused=mode=="grant-pause" || mode=="request-pause";if(paused)probe.arm(controller,mode=="grant-pause");std::atomic<bool> watcherStop=false;std::thread watcher;
 if(paused)watcher=std::thread([&]{bool notified=false;while(!watcherStop){if(probe.ready && !notified){std::ofstream(root/"probe-ready").put('1');notified=true;}if(std::filesystem::exists(root/"release-probe"))probe.release=true;Sleep(1);}});
 if(!controller.enableStageLink(endpoint,error) || !controller.start(TEST_MODULE_ROOT,root/"state",error,true)){std::cerr<<error;probe.release=true;watcherStop=true;if(watcher.joinable())watcher.join();return 3;}
 bool muted=false;while(!std::filesystem::exists(root/"stop-source")){
  if(!muted && std::filesystem::exists(root/"mute-source")){controller.mute(5);muted=true;}
  auto s=controller.snapshot();QJsonObject o{{"reason",QString::fromStdString(s.reason)},{"external",s.external},{"capturing",s.capturing},{"faults",static_cast<qint64>(s.faults)},{"blocks",static_cast<qint64>(s.blocks)},{"revision",static_cast<qint64>(s.state.revision)},{"mute",static_cast<int>(s.state.manualMuteMask)},{"session",QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(s.session.session_id),16).toHex())}};
  QSaveFile f(QString::fromStdWString((root/"source-status.json").wstring()));if(f.open(QIODevice::WriteOnly)){f.write(QJsonDocument(o).toJson());f.commit();}Sleep(10);
 }probe.release=true;watcherStop=true;if(watcher.joinable())watcher.join();controller.stop();return 0;
}
