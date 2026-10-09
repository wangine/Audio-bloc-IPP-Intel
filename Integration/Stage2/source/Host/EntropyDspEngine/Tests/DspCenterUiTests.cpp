#include "DspGraphPage.h"
#include "DspStyle.h"
#include "DspSessionDiscovery.h"
#include "TestSupport.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QSlider>
#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include <QFontDatabase>
#include <cmath>
int main(int argc,char**argv){
 QApplication app(argc,argv);QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
 QTemporaryDir tmp;auto root=std::filesystem::path(tmp.path().toStdWString());
 DspController controller;CenterSettings settings;std::string error;
 CHECK(controller.configureCenter(CENTER_MODULE_ROOT,settings,error));
 auto sessionHeader=session(1);StageBusWin32 source,consumer;
 CHECK(source.create(sessionHeader,error));CHECK(DspSessionDiscovery::write(root/"input.json",sessionHeader,"EXTERNAL",error));CHECK(controller.start(root,root/"input.json",error));
 auto wait=[&](auto predicate){auto deadline=qpc()+frequency()*3;while(!predicate()){if(qpc()>=deadline||controller.snapshot().state==DspStateCode::Fault)return false;source.heartbeat(qpc());consumer.heartbeat(qpc());app.processEvents();Sleep(1);}return true;};
 CHECK(wait([&]{return controller.snapshot().diag.center.loaded;}));CHECK(consumer.open(controller.snapshot().outputSession,error));
 Planes<10> in;Planes<11> out;uint64_t sequence=0;
 auto cycle=[&](bool silence=false){auto seq=++sequence;for(unsigned i=0;i<1024;i++){in.data[0][i]=in.data[1][i]=silence?0:.25*std::sin(((seq-1)*1024+i)*.03);}
 if(consumer.trySendRequest(request(sessionHeader,seq))!=StageBusIoResult::Ok)return false;StageBusFrameRequest req{};if(!wait([&]{return source.tryReceiveRequest(req)==StageBusIoResult::Ok;}))return false;
 auto h=header(req);if(source.tryPublish(h,in.in.data())!=StageBusIoResult::Ok)return false;StageBusBlockHeader result{};if(!wait([&]{return consumer.tryConsume(result,out.out.data())==StageBusIoResult::Ok;}))return false;
 return !std::memcmp(&h,&result,sizeof(h))&&!std::memcmp(in.in[0],out.in[0],8192)&&!std::memcmp(in.in[1],out.in[1],8192);};
 CHECK(cycle());CHECK(wait([&]{return controller.snapshot().diag.center.latencyValid;}));
 DspGraphPage graph(root);entropy::host::applySourceStyle(graph);graph.bindController(controller);graph.resize(1360,900);graph.show();graph.openModule(DspModuleKind::Center);app.processEvents();auto* dialog=graph.moduleDialog(DspModuleKind::Center);CHECK(dialog);
 auto* threshold=dialog->findChild<QSlider*>("center.threshold");auto* gain=dialog->findChild<QSlider*>("center.centerGain");auto* mode=dialog->findChild<QComboBox*>("center.mode");auto* detector=dialog->findChild<QComboBox*>("center.detectorMode");CHECK(threshold&&gain&&mode&&detector&&threshold->isEnabled());
 threshold->setSliderDown(true);
 for(int value=451;value<481;value++){
  threshold->setValue(value);CHECK(wait([&]{return controller.snapshot().diag.center.settings.values[2]==double(float(value/1000.0));}));
  CHECK(controller.snapshot().diag.center.latencyValid);graph.refresh(controller.snapshot(),qpc());CHECK(threshold->value()==value);CHECK(dialog->findChild<QLabel*>("center.commandState")->text()=="CONTROLS APPLIED");CHECK(cycle());
 }
 threshold->setSliderDown(false);
 // Acknowledged timing context must not reuse old detector telemetry.
 mode->setCurrentIndex(0);CHECK(wait([&]{return controller.snapshot().diag.center.settings.values[0]==0;}));
 mode->setCurrentIndex(1);CHECK(wait([&]{return controller.snapshot().diag.center.settings.values[0]==1;}));
 CHECK(!controller.snapshot().diag.center.latencyValid);graph.refresh(controller.snapshot(),qpc());
 CHECK(dialog->findChild<QProgressBar*>("center.activity0")->value()==0);
 CHECK(cycle());CHECK(wait([&]{return controller.snapshot().diag.center.latencyValid;}));
 auto stale=controller.snapshot();stale.diag.signal.publishedQpc=qpc()-frequency();graph.refresh(stale,qpc());
 CHECK(dialog->findChild<QProgressBar*>("center.activity0")->value()==0);

 detector->setCurrentIndex(2);CHECK(!dialog->findChild<QSlider*>("center.midWeight")->isEnabled());CHECK(dialog->findChild<QSlider*>("center.levelWeight")->isEnabled());
 mode->setCurrentIndex(0);CHECK(!threshold->isEnabled());CHECK(wait([&]{return controller.snapshot().diag.center.settings.values[0]==0;}));CHECK(cycle());CHECK(wait([&]{return controller.snapshot().diag.blocksOut==sequence;}));graph.refresh(controller.snapshot(),qpc());CHECK(dialog->findChild<QLabel*>("center.activityText0")->text()=="—");
 // SUM at explicit 0 dB is unity for identical LR. Gain is real and only affects C.
 for(unsigned i=0;i<1024;i++)CHECK(out.data[2][i]==in.data[0][i]);
 gain->setValue(-600);CHECK(wait([&]{return controller.snapshot().diag.center.settings.values[14]==-6;}));
 for(unsigned b=0;b<8;b++)CHECK(cycle());const double scale=std::pow(10.,-6./20.);
 for(unsigned i=0;i<1024;i++)CHECK(std::abs(out.data[2][i]-in.data[0][i]*scale)<2e-8);
 gain->setValue(0);mode->setCurrentIndex(1);detector->setCurrentIndex(0);CHECK(wait([&]{return controller.snapshot().diag.center.settings.values[0]==1&&controller.snapshot().diag.center.settings.values[14]==0;}));
 for(unsigned b=0;b<12;b++)CHECK(cycle());CHECK(cycle(true));CHECK(wait([&]{return controller.snapshot().diag.blocksOut==sequence;}));graph.refresh(controller.snapshot(),qpc());
 for(unsigned i=0;i<4;i++)CHECK(dialog->findChild<QProgressBar*>(QString("center.activity%1").arg(i))->value()==0);
 if(argc>1)dialog->grab().save(QString::fromLocal8Bit(argv[1]));dialog->close();controller.stop();
 std::cout<<"PASS independent StageBus fixture; 30 drag edits acknowledged without latency flicker; real C-only gain; SUM/SPECTRAL control context; stale similarity hidden on silence\n";
}
