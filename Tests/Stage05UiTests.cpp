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
#include <QSlider>
#include <QDoubleSpinBox>
#include <QLabel>
#include <entropy/headroom_abi.h>
#include <QGridLayout>
#include <QSpinBox>
#include <algorithm>
#include <cmath>
template<class T>static T* headroomControl(QWidget* page,const QString& label,int column=1){
 for(auto* text:page->findChildren<QLabel*>())if(text->text()==label){
  auto* grid=qobject_cast<QGridLayout*>(text->parentWidget()->layout());if(!grid)continue;
  int row=0,col=0,rows=0,cols=0;grid->getItemPosition(grid->indexOf(text),&row,&col,&rows,&cols);
  auto* item=grid->itemAtPosition(row,column);return item?qobject_cast<T*>(item->widget()):nullptr;
 }return nullptr;
}
int main(int argc,char** argv){
 QApplication app(argc,argv);if(QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf")<0){std::cerr<<"System Segoe UI font unavailable\n";return 1;}QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeuisb.ttf");entropy::ui::EngineFacade facade(argc>1?QString::fromUtf8(argv[1]):"EntropyAudioCore.Control.v1");entropy::stage::RenderWindow window(facade);window.show();
 QTimer::singleShot(800,&app,[&]{
  bool good=facade.connected();for(unsigned i=0;i<4;++i){window.openModule(entropy::stage::RenderPage(i));good=good&&window.moduleDialog(entropy::stage::RenderPage(i));}
  auto* tabs=window.moduleDialog(entropy::stage::RenderPage::Crossover)->findChild<QTabWidget*>("crossoverTabs");good=good&&tabs;good=good&&window.findChild<QPushButton*>("renderProgramShortcut")&&window.findChild<QPushButton*>("renderOutputShortcut");
  auto* crossover=window.moduleDialog(entropy::stage::RenderPage::Crossover);
  good=good&&crossover->findChild<QSlider*>("fineDelaySlider")&&crossover->findChild<QSlider*>("coarseDelaySlider")&&!crossover->findChild<QDoubleSpinBox*>("preciseRelativeDelay");
  for(auto* text:crossover->findChildren<QLabel*>())good=good&&!text->text().contains("GLOBAL BASE")&&!text->text().contains("PRECISE RELATIVE DELAY");
  const bool initialContract=good;
  using namespace entropy::host;uint64_t sequence=0;
  good=good&&writeEngineParameterIntent("audio.entropy.crossover","cinemaSource",1);
  good=good&&writeEngineParameterIntent("audio.entropy.crossover","cinemaDelayMs1",3);
  good=good&&writeEngineOutputMatrixClearAll(sequence)&&writeEngineOutputMatrixRoute(0,1,true,sequence);
  auto* headroom=window.moduleDialog(entropy::stage::RenderPage::Headroom);
  auto* mode=headroomControl<QComboBox>(headroom,"MODE");
  auto* manual=headroomControl<QDoubleSpinBox>(headroom,"MANUAL GAIN / AUTO MAX");
  auto* reserve=headroomControl<QDoubleSpinBox>(headroom,"AUTO PREVENTIVE RESERVE");
  auto* transition=headroomControl<QDoubleSpinBox>(headroom,"MODE / MANUAL TRANSITION");
  auto* attack=headroomControl<QSlider>(headroom,"PREVENTIVE ATTACK");
  auto* release=headroomControl<QSlider>(headroom,"RELEASE");
  good=good&&mode&&manual&&reserve&&transition&&attack&&release;
  good=good&&headroomControl<QLabel>(headroom,"SAMPLE SAFETY REQUIRED")&&headroomControl<QLabel>(headroom,"AUTO CAPACITY")&&headroomControl<QLabel>(headroom,"ANALYZER");
  if(!good){std::cerr<<"V07 global16 Headroom page controls missing: connected="<<facade.connected()<<" initialContract="<<initialContract
   <<" crossoverTabs="<<(tabs!=nullptr)<<" fineDelay="<<(crossover->findChild<QSlider*>("fineDelaySlider")!=nullptr)<<" coarseDelay="<<(crossover->findChild<QSlider*>("coarseDelaySlider")!=nullptr)
   <<" mode="<<(mode!=nullptr)<<" manual="<<(manual!=nullptr)<<" reserve="<<(reserve!=nullptr)<<" transition="<<(transition!=nullptr)<<" attack="<<(attack!=nullptr)<<" release="<<(release!=nullptr)
   <<" sampleSafety="<<(headroomControl<QLabel>(headroom,"SAMPLE SAFETY REQUIRED")!=nullptr)<<" capacity="<<(headroomControl<QLabel>(headroom,"AUTO CAPACITY")!=nullptr)<<" analyzer="<<(headroomControl<QLabel>(headroom,"ANALYZER")!=nullptr)<<"\n";
   for(auto* text:headroom->findChildren<QLabel*>()){auto* layout=text->parentWidget()->layout();std::cerr<<"Headroom label '"<<text->text().toStdString()<<"' layout="<<(layout?layout->metaObject()->className():"none")<<"\n";}
   app.exit(1);return;}
  mode->setCurrentIndex(mode->findData(ENTROPY_HEADROOM_MODE_AUTO));
  manual->setValue(-12);reserve->setValue(3);transition->setValue(5);
  good=good&&QMetaObject::invokeMethod(reserve,"editingFinished",Qt::DirectConnection)&&QMetaObject::invokeMethod(transition,"editingFinished",Qt::DirectConnection);
  attack->setValue(600);release->setValue(3000);
  good=good&&QMetaObject::invokeMethod(attack,"sliderReleased",Qt::DirectConnection)&&QMetaObject::invokeMethod(release,"sliderReleased",Qt::DirectConnection);
  mode->setCurrentIndex(mode->findData(ENTROPY_HEADROOM_MODE_MANUAL_SAFE));
  good=good&&writeEngineCrossoverMonitorChannel(0)&&writeEngineCrossoverResetAllProtectionTrips();if(!good){std::cerr<<"Qt UI/IPC initial contract FAIL\n";app.exit(1);return;}
  auto* delayMode=window.moduleDialog(entropy::stage::RenderPage::Crossover)->findChild<QComboBox*>("fractionalDelayMode");
  if(!delayMode||delayMode->count()!=4){std::cerr<<"Qt fractional delay selector missing\n";app.exit(1);return;}
  delayMode->setCurrentIndex(3); // UI -> IPC -> immutable Core snapshot
  QTimer::singleShot(1300,&app,[&]{entropy::host::EngineTelemetrySnapshot s{};bool pass=facade.bridge().read(s)&&s.crossoverCinemaProgram==1&&s.crossoverParameterCount>400&&!s.outputArmed&&s.outputMatrixSourceDestinationMask[0]==2&&s.outputMatrixRouteCount==1&&(s.outputMatrixOrphanedSourceMask&s.outputMatrixActiveSourceMask)==(s.outputMatrixActiveSourceMask&0xfffe)&&s.headroomGroupCount==1&&s.headroomPreventiveReserveDb[0]==3&&s.headroomFixedAttenuationDb==-12&&s.headroomAutoAttackMs==600&&s.headroomAutoReleaseMs==3000;
  pass=pass&&s.headroomHostEngineRate==192000&&s.headroomTransitionSamples==960&&s.headroomProcessedFrames>0&&s.headroomNonfiniteSampleCount==0&&s.headroomFaultFlags==0&&s.headroomAppliedRevision==s.headroomStateRevision&&s.headroomGroupBounded[0]==1&&s.headroomActiveChannelMask==0xffff;
  pass=pass&&s.headroomRequestedMode==ENTROPY_HEADROOM_MODE_MANUAL_SAFE&&s.headroomEffectiveMode==ENTROPY_HEADROOM_MODE_MANUAL_SAFE&&!s.headroomModeTransitionActive;
  pass=pass&&std::abs(s.headroomFixedTargetGain-std::pow(10.,-12./20.))<1e-12&&std::abs(s.headroomCurrentGain[0]-s.headroomFixedTargetGain)<1e-12;
  pass=pass&&s.headroomGuaranteedInputDb[0]==0&&std::abs(s.headroomRequiredDb[0]-s.headroomDeclaredPositiveDb[0]-s.headroomPreventiveReserveDb[0])<1e-12;
  double headroomPeak=0;for(unsigned channel=0;channel<16;++channel){pass=pass&&s.headroomChannelGroup[channel]==0&&s.headroomChannelInputPeak[channel]==s.outputMatrixDestinationPeaks[channel];headroomPeak=std::max(headroomPeak,s.headroomChannelInputPeak[channel]);}
  pass=pass&&headroomPeak>0&&headroomPeak==s.headroomMeasuredInputPeak&&s.headroomTruePeakStatus==0;
  auto* headroomPage=window.moduleDialog(entropy::stage::RenderPage::Headroom);
  auto* capacity=headroomControl<QLabel>(headroomPage,"AUTO CAPACITY");auto* applied=headroomControl<QLabel>(headroomPage,"APPLIED ATTENUATION");
  pass=pass&&capacity&&capacity->text()=="N/A"&&applied&&applied->text().startsWith("12.00")&&!headroomControl<QSlider>(headroomPage,"PREVENTIVE ATTACK")->isEnabled();
  double peak=0;for(auto value:s.crossoverOutputPeaks)peak=std::max(peak,value);pass=pass&&peak>1e-8;
  pass=pass&&s.crossoverFractionalDelayOrder[0]==5&&!s.crossoverFractionalDelayTransition[0];
  for(unsigned i=0;i<s.crossoverParameterCount;++i)if(QString::fromUtf8(s.crossoverParameters[i].identifier)=="cinemaDelayMs1"){std::cout<<std::setprecision(17)<<"Delay observed="<<s.crossoverParameters[i].value<<"\n";pass=pass&&s.crossoverParameters[i].value==3.00006103515625; /* Exact original native normalized float control round-trip. */}std::cout<<"Observed program="<<s.crossoverCinemaProgram<<" route="<<s.outputMatrixSourceDestinationMask[0]<<" groups="<<s.headroomGroupCount<<" membership="<<s.headroomChannelGroup[0]<<" reserve="<<s.headroomPreventiveReserveDb[0]<<" gain="<<s.headroomFixedAttenuationDb<<" attack="<<s.headroomAutoAttackMs<<" release="<<s.headroomAutoReleaseMs<<" peak="<<peak<<" error="<<s.outputDetail<<"\n";QDir().mkpath("local/stage05/ui-captures");window.grab().save("local/stage05/ui-captures/overview.png");for(unsigned i=0;i<4;++i)window.moduleDialog(entropy::stage::RenderPage(i))->grab().save(QString("local/stage05/ui-captures/module-%1.png").arg(i));std::cout<<(pass?"Qt original pages + two-process control PASS\n":"Qt two-process control FAIL\n");if(!pass){app.exit(1);return;}
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
