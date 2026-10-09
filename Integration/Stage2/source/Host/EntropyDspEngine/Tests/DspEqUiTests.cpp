#include "DspGraphPage.h"
#include "DspSessionDiscovery.h"
#include "TestSupport.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QFontDatabase>
#include <cmath>
int main(int argc,char**argv){
 QApplication app(argc,argv);QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");QTemporaryDir tmp;auto root=std::filesystem::path(tmp.path().toStdWString());DspController controller;std::string error;CenterSettings center;center.values[0]=0;
 CHECK(controller.configureCenter(CENTER_MODULE_ROOT,center,error));CHECK(controller.configureEq(CENTER_MODULE_ROOT,defaultEqSettings(),error));
 auto s=session(1);StageBusWin32 source,consumer;CHECK(source.create(s,error));CHECK(DspSessionDiscovery::write(root/"input.json",s,"EXTERNAL",error));CHECK(controller.start(root,root/"input.json",error));
 auto wait=[&](auto predicate){auto until=qpc()+frequency()*3;while(!predicate()){if(qpc()>until||controller.snapshot().state==DspStateCode::Fault)return false;source.heartbeat(qpc());consumer.heartbeat(qpc());app.processEvents();Sleep(1);}return true;};
 CHECK(wait([&]{return controller.snapshot().diag.eq.loaded;}));CHECK(consumer.open(controller.snapshot().outputSession,error));Planes<10> in;Planes<11> out;uint64_t sequence=0;
 auto cycle=[&](){auto seq=++sequence;for(unsigned i=0;i<1024;i++){double t=(seq-1)*1024+i;in.data[0][i]=in.data[1][i]=.1*std::sin(t*2*3.141592653589793*1000/192000);}if(consumer.trySendRequest(request(s,seq))!=StageBusIoResult::Ok)return false;StageBusFrameRequest req{};if(!wait([&]{return source.tryReceiveRequest(req)==StageBusIoResult::Ok;}))return false;auto h=header(req);if(source.tryPublish(h,in.in.data())!=StageBusIoResult::Ok)return false;StageBusBlockHeader result{};return wait([&]{return consumer.tryConsume(result,out.out.data())==StageBusIoResult::Ok;});};
 CHECK(cycle());DspGraphPage graph(root);graph.bindController(controller);graph.resize(1360,900);graph.show();graph.openModule(DspModuleKind::Eq);app.processEvents();auto* dialog=graph.moduleDialog(DspModuleKind::Eq);CHECK(dialog);auto* mode=dialog->findChild<QPushButton*>("mode_static");CHECK(mode&&mode->isEnabled());
 auto* gain=dialog->findChild<QDoubleSpinBox*>("band0_staticGainDb");auto* hz=dialog->findChild<QDoubleSpinBox*>("band0_frequencyHz");CHECK(gain&&hz);hz->setValue(1000);gain->setValue(6);mode->click();
 auto settle=[&](double expected){auto until=qpc()+frequency()*3;while(qpc()<until){if(!cycle())return false;app.processEvents();Sleep(8);auto state=controller.snapshot().diag.eq;if(state.settings.mode==1&&state.settings.bands[0].static_gain_db==expected&&state.appliedRevision==state.revision&&dialog->findChild<QLabel*>("parametricApplyState")->text()=="APPLIED")return true;}return false;};
 CHECK(settle(6));double dry=0,wet=0;for(unsigned b=0;b<24;b++){CHECK(cycle());for(unsigned i=0;i<1024;i++){CHECK(out.data[2][i]==in.data[0][i]);if(b>12){dry+=in.data[0][i]*in.data[0][i];wet+=out.data[0][i]*out.data[0][i];}}}CHECK(std::sqrt(wet/dry)>1.9);
 CHECK(dialog->findChild<QLabel*>("parametricStatus")->text()=="ACTIVE / FP64");
 for(int i=4;i<=10;i++)gain->setValue(i);CHECK(settle(10));CHECK(gain->value()==10);
 // Settings remain editable/saveable when no new audio blocks are delivered.
 gain->setValue(11);CHECK(wait([&]{EqSettings requested{};return controller.requestedEqSettings(requested)&&requested.bands[0].static_gain_db==11;}));
 gain->setValue(12);EqSettings newest{};CHECK(controller.requestedEqSettings(newest)&&newest.bands[0].static_gain_db==12);
 if(argc>1)dialog->grab().save(QString::fromLocal8Bit(argv[1]));dialog->close();controller.stop();std::cout<<"PASS Stage2 EQ UI native acknowledgements, +6 dB real LR transfer, raw Center unchanged, burst latest edit retained\n";
}
