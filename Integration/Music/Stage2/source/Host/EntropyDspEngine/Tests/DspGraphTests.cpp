#include "DspGraphPage.h"
#include "TestSupport.h"
#include <QApplication>
#include <QFontDatabase>
#include <QEventLoop>
#include <QTimer>
#include <QLabel>
#include <QImage>
void waitUi(int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();}
int main(int argc,char** argv){QApplication app(argc,argv);QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");DspGraphPage graph({});graph.resize(1300,800);graph.show();app.processEvents();auto* in=graph.inputMatrix();auto* out=graph.outputMatrix();CHECK(in && out);CHECK(in->inputCount()==10 && out->outputCount()==11);DspUiSnapshot s{};s.state=DspStateCode::Running;s.outputSession.qpc_frequency=frequency();s.diag.signal.publishedQpc=qpc();s.diag.signal.inputMask=1;s.diag.signal.outputMask=1;graph.refresh(s,qpc());in->setPulsePhase(0);auto a=in->grab().toImage();in->setPulsePhase(1.57);auto b=in->grab().toImage();CHECK(a!=b);CHECK(a.pixelColor(13,24)!=b.pixelColor(13,24));CHECK(a.pixelColor(13,74)==b.pixelColor(13,74));CHECK(!(out->outputMask()&4));
 auto phase=in->pulsePhase();graph.hide();waitUi(100);CHECK(in->pulsePhase()==phase);graph.show();graph.showMinimized();phase=in->pulsePhase();waitUi(100);CHECK(in->pulsePhase()==phase);graph.showNormal();waitUi(100);CHECK(in->pulsePhase()!=phase);
 graph.refresh(s,s.diag.signal.publishedQpc+frequency());CHECK(!in->inputMask() && !out->outputMask());graph.refresh(s,s.diag.signal.publishedQpc);s.state=DspStateCode::Fault;graph.refresh(s,qpc());CHECK(!in->inputMask());
 if(argc>1){graph.refresh(s,qpc());graph.grab().save(QString::fromLocal8Bit(argv[1]));}
 std::cout<<"PASS original matrix pulse, correct lane counts, active-only pixels, CX off, stale/fault clear and hidden/minimized suspension\n";
}
