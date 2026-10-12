#include "DspWindow.h"
#include "DspState.h"
#include "DspVersion.h"
#include "DspGraphPage.h"
#include "TestSupport.h"
#include <QApplication>
#include <QFileDialog>
#include <QTemporaryDir>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QFontDatabase>
#include <QMessageBox>
#include <QFile>
#include <QPointer>
int main(int argc,char** argv){
 QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);QApplication app(argc,argv);
 QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
 QTemporaryDir tmp;auto root=std::filesystem::path(tmp.path().toStdWString());DspController controller;
 CenterSettings settings;settings.values[13]=4.5;settings.values[14]=2.25;std::string error;
 CHECK(controller.configureCenter(root/"modules",settings,error));
 CHECK(controller.configureEq(root/"modules",defaultEqSettings(),error));
 DspState initial;initial.importedProject="original archived bytes";CHECK(initial.save(root/"state/Stage2State.json",error));
 DspWindow window(controller,root,false);window.resize(1360,900);window.show();app.processEvents();
 CHECK(window.windowTitle()==QString("DSP Engine · %1").arg(dspVersion));CHECK(!window.findChild<QLabel*>("dspStatus"));
 CHECK(window.findChild<QLabel*>("brand")->text()==window.windowTitle());
 auto* save=window.findChild<QPushButton*>("dspSave");CHECK(save);
 auto* load=window.findChild<QPushButton*>("dspLoad");CHECK(load);
 bool cancelled=false,defaultCorrect=false;
 QTimer::singleShot(50,[&]{auto* dialog=window.findChild<QFileDialog*>("dspSaveDialog");if(!dialog)return;defaultCorrect=dialog->directory().absolutePath()==QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).absolutePath()&&dialog->defaultSuffix()=="Edsp";cancelled=true;dialog->reject();});save->click();CHECK(cancelled&&defaultCorrect);
 DspState unchanged;CHECK(unchanged.load(root/"state/Stage2State.json",error));CHECK(!unchanged.centerConfigured);
 auto selected=QDir(tmp.path()).filePath(QString::fromUtf8("Profil muzică"));bool accepted=false;
 QTimer::singleShot(50,[&]{auto* dialog=window.findChild<QFileDialog*>("dspSaveDialog");if(!dialog)return;auto* edit=dialog->findChild<QLineEdit*>("fileNameEdit");if(!edit)return;edit->setText(selected);accepted=QMetaObject::invokeMethod(dialog,"accept",Qt::DirectConnection);});save->click();CHECK(accepted);CHECK(save->text()=="SAVED");
 std::cout<<"Saved path: "<<save->toolTip().toStdString()<<" expected: "<<selected.toStdString()<<"\n";DspState external,cache;CHECK(external.load(std::filesystem::path((selected+".Edsp").toStdWString()),error));CHECK(cache.load(root/"state/Stage2State.json",error));CHECK(external.centerConfigured&&external.centerSettings.values==settings.values);CHECK(cache.centerSettings.values==settings.values);CHECK(external.importedProject==initial.importedProject);
 QString remembered;std::string warning;DspState startup;CHECK(startup.loadStartup(root,remembered,warning,error));CHECK(remembered==selected+".Edsp"&&warning.empty());
 bool loadCancelled=false;QTimer::singleShot(50,[&]{if(auto* dialog=window.findChild<QFileDialog*>("dspLoadDialog")){loadCancelled=true;dialog->reject();}});load->click();CHECK(loadCancelled);
 auto second=QDir(tmp.path()).filePath(QString::fromUtf8("Alt profil stereo.Edsp"));external.centerSettings.values[13]=6.25;external.eqSettings.bands[0].static_gain_db=-3;external.alignmentActive=true;CHECK(external.save(std::filesystem::path(second.toStdWString()),error));
 auto* graph=dynamic_cast<DspGraphPage*>(window.findChild<QWidget*>("dspGraphPage"));CHECK(graph);graph->openModule(DspModuleKind::Center);QPointer<QDialog> previous=graph->moduleDialog(DspModuleKind::Center);CHECK(previous);
 bool loadAccepted=false;QTimer::singleShot(50,[&]{if(auto* dialog=window.findChild<QFileDialog*>("dspLoadDialog")){if(auto* edit=dialog->findChild<QLineEdit*>("fileNameEdit"))edit->setText(second);loadAccepted=QMetaObject::invokeMethod(dialog,"accept",Qt::DirectConnection);}});load->click();CHECK(loadAccepted&&load->text()=="LOADED"&&previous.isNull());
 CenterSettings loadedCenter;EqSettings loadedEq{};controller.requestedCenterSettings(loadedCenter);std::cout<<"Loaded "<<load->toolTip().toStdString()<<" center="<<loadedCenter.values[13]<<" expected="<<external.centerSettings.values[13]<<"\n";CHECK(controller.requestedCenterSettings(loadedCenter)&&loadedCenter.values==external.centerSettings.values);CHECK(controller.requestedEqSettings(loadedEq)&&loadedEq.bands[0].static_gain_db==-3);
 CHECK(startup.loadStartup(root,remembered,warning,error));CHECK(remembered==second&&startup.alignmentActive==true&&startup.centerSettings.values==external.centerSettings.values);
 QFile broken(QDir(tmp.path()).filePath("broken.Edsp"));CHECK(broken.open(QIODevice::WriteOnly));broken.write("not a profile");broken.close();
 bool rejected=false;QTimer dismiss;dismiss.setInterval(10);QObject::connect(&dismiss,&QTimer::timeout,[&]{if(auto* message=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){rejected=true;message->accept();}});dismiss.start();
 QTimer::singleShot(50,[&]{if(auto* dialog=window.findChild<QFileDialog*>("dspLoadDialog")){if(auto* edit=dialog->findChild<QLineEdit*>("fileNameEdit"))edit->setText(broken.fileName());QMetaObject::invokeMethod(dialog,"accept",Qt::DirectConnection);}});load->click();dismiss.stop();CHECK(rejected);
 CHECK(controller.requestedCenterSettings(loadedCenter)&&loadedCenter.values==external.centerSettings.values);CHECK(startup.loadStartup(root,remembered,warning,error)&&remembered==second);
 if(argc>1)window.grab().save(QString::fromLocal8Bit(argv[1]));
 window.close();std::cout<<"PASS LOAD/SAVE dialogs, Unicode paths, cancel/invalid retain settings, Center/EQ requests, stale views destroyed, remembered startup profile and LRC intent\n";
}
