#include "Client/EngineFacade.h"
#include "Legacy/RenderWindow.h"
#include <QApplication>
#include <QTimer>
#include <QTabWidget>
#include <QPushButton>
#include <QDir>
#include <QFontDatabase>
#include <iostream>
#include <iomanip>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QComboBox>
int main(int argc,char** argv){
 QApplication app(argc,argv);if(QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf")<0){std::cerr<<"System Segoe UI font unavailable\n";return 1;}QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeuisb.ttf");entropy::ui::EngineFacade facade(argc>1?QString::fromUtf8(argv[1]):"EntropyAudioCore.Control.v1");entropy::stage::RenderWindow window(facade);window.show();
 QTimer::singleShot(800,&app,[&]{
  bool good=facade.connected();for(unsigned i=0;i<4;++i){window.openModule(entropy::stage::RenderPage(i));good=good&&window.moduleDialog(entropy::stage::RenderPage(i));}
  auto* tabs=window.moduleDialog(entropy::stage::RenderPage::Crossover)->findChild<QTabWidget*>("crossoverTabs");good=good&&tabs;good=good&&window.findChild<QPushButton*>("renderProgramShortcut")&&window.findChild<QPushButton*>("renderOutputShortcut");
  using namespace entropy::host;uint64_t sequence=0;
  good=good&&writeEngineParameterIntent("audio.entropy.crossover","cinemaSource",1);
  good=good&&writeEngineParameterIntent("audio.entropy.crossover","cinemaDelayMs1",3);
  good=good&&writeEngineOutputMatrixClearAll(sequence)&&writeEngineOutputMatrixRoute(0,1,true,sequence);
  good=good&&writeEngineHeadroomGroupCount(2,sequence)&&writeEngineHeadroomChannelGroup(0,1,sequence)&&writeEngineHeadroomPreventiveReserve(1,3,sequence);
  good=good&&writeEngineHeadroomOperatingPolicy(2,-12,sequence)&&writeEngineHeadroomTransitionMs(5,sequence)&&writeEngineHeadroomAutoAttackMs(600,sequence)&&writeEngineHeadroomAutoReleaseMs(3000,sequence);
  good=good&&writeEngineCrossoverMonitorChannel(0)&&writeEngineCrossoverResetAllProtectionTrips();if(!good){std::cerr<<"Qt UI/IPC initial contract FAIL\n";app.exit(1);return;}
  auto* delayMode=window.moduleDialog(entropy::stage::RenderPage::Crossover)->findChild<QComboBox*>("fractionalDelayMode");
  if(!delayMode||delayMode->count()!=4){std::cerr<<"Qt fractional delay selector missing\n";app.exit(1);return;}
  delayMode->setCurrentIndex(3); // UI -> IPC -> immutable Core snapshot
  QTimer::singleShot(1300,&app,[&]{entropy::host::EngineTelemetrySnapshot s{};bool pass=facade.bridge().read(s)&&s.crossoverCinemaProgram==1&&s.crossoverParameterCount>400&&!s.outputArmed&&s.outputMatrixSourceDestinationMask[0]==2&&s.outputMatrixRouteCount==1&&s.outputMatrixOrphanedSourceMask==0xfffe&&s.headroomGroupCount==2&&s.headroomChannelGroup[0]==1&&s.headroomPreventiveReserveDb[1]==3&&s.headroomFixedAttenuationDb==-12&&s.headroomAutoAttackMs==600&&s.headroomAutoReleaseMs==3000;
  double peak=0;for(auto value:s.crossoverOutputPeaks)peak=std::max(peak,value);pass=pass&&peak>1e-8;
  pass=pass&&s.crossoverFractionalDelayOrder[0]==5&&!s.crossoverFractionalDelayTransition[0];
  for(unsigned i=0;i<s.crossoverParameterCount;++i)if(QString::fromUtf8(s.crossoverParameters[i].identifier)=="cinemaDelayMs1"){std::cout<<std::setprecision(17)<<"Delay observed="<<s.crossoverParameters[i].value<<"\n";pass=pass&&s.crossoverParameters[i].value==3.00006103515625; /* Exact original native normalized float control round-trip. */}std::cout<<"Observed program="<<s.crossoverCinemaProgram<<" route="<<s.outputMatrixSourceDestinationMask[0]<<" groups="<<s.headroomGroupCount<<" membership="<<s.headroomChannelGroup[0]<<" reserve="<<s.headroomPreventiveReserveDb[1]<<" gain="<<s.headroomFixedAttenuationDb<<" attack="<<s.headroomAutoAttackMs<<" release="<<s.headroomAutoReleaseMs<<" peak="<<peak<<" error="<<s.outputDetail<<"\n";QDir().mkpath("local/stage05/ui-captures");window.grab().save("local/stage05/ui-captures/overview.png");for(unsigned i=0;i<4;++i)window.moduleDialog(entropy::stage::RenderPage(i))->grab().save(QString("local/stage05/ui-captures/module-%1.png").arg(i));std::cout<<(pass?"Qt original pages + two-process control PASS\n":"Qt two-process control FAIL\n");if(!pass){app.exit(1);return;}
   auto path=QDir::current().absoluteFilePath("local/stage05/ui-profile.entropy");
   facade.request({{"action","save-profile"},{"path",path}},[&,path](const QJsonObject& reply){
    if(!reply["ok"].toBool()){std::cerr<<"UI profile save FAIL\n";app.exit(1);return;}
    QFile input(path);if(!input.open(QIODevice::ReadOnly)){app.exit(1);return;}auto document=QJsonDocument::fromJson(input.readAll()).object();input.close();auto devices=document["stage05_devices"].toObject();
    devices["bindings"]=QJsonArray{QJsonObject{{"logicalId","DAC_A"},{"endpoint","offline-profile-fixture"},{"name","RESTORED PROFILE DAC"},{"fingerprint","offline-test"},{"approvalRevision","42"},{"evidence",QJsonArray{}},{"formats",QJsonArray{}}},QJsonValue::Null};document["stage05_devices"]=devices;
    QFile fixture(path);if(!fixture.open(QIODevice::WriteOnly)){app.exit(1);return;}fixture.write(QJsonDocument(document).toJson());fixture.close();uint64_t sequence=0;
    if(!entropy::host::writeEngineProjectRequest(entropy::host::EngineProjectLoad,path.toStdWString().c_str(),sequence)){app.exit(1);return;}
    QTimer::singleShot(1000,&app,[&]{auto bindings=facade.devices()["bindings"].toArray();bool refreshed=bindings.size()==2&&bindings[0].toObject()["name"].toString()=="RESTORED PROFILE DAC";std::cout<<(refreshed?"Qt profile DAC cache refresh PASS\n":"Qt profile DAC cache refresh FAIL\n");app.exit(refreshed?0:1);});
   });});
 });return app.exec();
}
