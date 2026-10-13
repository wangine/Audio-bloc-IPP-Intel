#include "DspGraphPage.h"
#include "DspSignalTelemetry.h"
#include "DspStyle.h"
#include "TestSupport.h"
#include <QApplication>
#include <QLabel>
#include <QFontDatabase>
#include <QPushButton>
#include <QSlider>
int main(int argc,char** argv){
 QApplication app(argc,argv);QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR")+"/Fonts/segoeui.ttf");app.setFont(QFont("Segoe UI"));DspGraphPage page({});entropy::host::applySourceStyle(page);auto input=dynamic_cast<MatrixGraphNode*>(page.findChild<QWidget*>("matrixIn")),output=dynamic_cast<MatrixGraphNode*>(page.findChild<QWidget*>("matrixOut"));
 CHECK(input && output && input->inputCount()==2 && input->outputCount()==2);
 CHECK(output->inputCount()==3 && output->outputCount()==3);
 for(auto label:page.findChildren<QLabel*>())CHECK(!label->text().contains("Cinema",Qt::CaseInsensitive));
 DspSignalSnapshot signal{};CHECK(signal.inputPeaks.size()==2 && signal.outputPeaks.size()==3);
 Planes<2> in;Planes<3> out;in.data[0].fill(.25);in.data[1].fill(-.5);out.data[0].fill(.25);out.data[1].fill(-.5);out.data[2].fill(.2);
 measureSignal(signal,in.in.data(),out.in.data(),3,qpc(),true);CHECK(signal.inputMask==3 && signal.outputMask==7);
 measureSignal(signal,in.in.data(),out.in.data(),0,qpc(),true);CHECK(!signal.inputMask && !signal.outputMask);
 DspUiSnapshot snapshot{};snapshot.state=DspStateCode::Running;snapshot.outputSession=session(4);snapshot.diag.signal=signal;snapshot.diag.alignment.loaded=1;
 page.refresh(snapshot,qpc());for(auto label:page.findChildren<QLabel*>())CHECK(!label->text().contains("Cinema",Qt::CaseInsensitive));
 DspController controller;std::string error;CHECK(controller.configureRecovery("modules",defaultRecoverySettings(),error));page.bindController(controller);
 page.openModule(DspModuleKind::Recovery);auto* dialog=page.moduleDialog(DspModuleKind::Recovery);CHECK(dialog);
 auto* enable=dialog->findChild<QPushButton*>("recovery.enabled");auto* strength=dialog->findChild<QSlider*>("recovery.strength");CHECK(enable&&strength);
 enable->click();strength->setValue(72);EntropyDynamicRecoveryConfigV1 requested{};CHECK(controller.requestedRecoverySettings(requested));CHECK(requested.enabled==1&&requested.transient_strength==.72);
 if(argc==3 && std::string(argv[1])=="--dialog-screenshot"){dialog->show();app.processEvents();CHECK(dialog->grab().save(QString::fromUtf8(argv[2])));}
 dialog->close();
 if(argc==3 && std::string(argv[1])=="--screenshot"){page.resize(1360,850);page.show();app.processEvents();CHECK(page.grab().save(QString::fromUtf8(argv[2])));}
 std::cout<<"PASS MUSIC UI 2/3 pins, no CINEMA cards, bounded meters and validity\n";
}
