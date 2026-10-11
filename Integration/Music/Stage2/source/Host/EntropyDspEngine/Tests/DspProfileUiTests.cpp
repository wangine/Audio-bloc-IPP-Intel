#include "DspWindow.h"
#include "DspState.h"
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
int main(int argc,char** argv){
 QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);QApplication app(argc,argv);
 QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
 QTemporaryDir tmp;auto root=std::filesystem::path(tmp.path().toStdWString());DspController controller;
 CenterSettings settings;settings.values[13]=4.5;settings.values[14]=2.25;std::string error;
 CHECK(controller.configureCenter(root/"modules",settings,error));
 DspState initial;initial.importedProject="original archived bytes";CHECK(initial.save(root/"state/Stage2State.json",error));
 DspWindow window(controller,root,false);window.resize(1360,900);window.show();app.processEvents();
 CHECK(window.windowTitle()=="DSP Engine · v0006");CHECK(!window.findChild<QLabel*>("dspStatus"));
 CHECK(window.findChild<QLabel*>("brand")->text()==window.windowTitle());
 auto* save=window.findChild<QPushButton*>("dspSave");CHECK(save);
 bool cancelled=false,defaultCorrect=false;
 QTimer::singleShot(50,[&]{auto* dialog=window.findChild<QFileDialog*>("dspSaveDialog");if(!dialog)return;defaultCorrect=dialog->directory().absolutePath()==QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).absolutePath()&&dialog->defaultSuffix()=="Edsp";cancelled=true;dialog->reject();});save->click();CHECK(cancelled&&defaultCorrect);
 DspState unchanged;CHECK(unchanged.load(root/"state/Stage2State.json",error));CHECK(!unchanged.centerConfigured);
 auto selected=QDir(tmp.path()).filePath(QString::fromUtf8("Profil muzică"));bool accepted=false;
 QTimer::singleShot(50,[&]{auto* dialog=window.findChild<QFileDialog*>("dspSaveDialog");if(!dialog)return;auto* edit=dialog->findChild<QLineEdit*>("fileNameEdit");if(!edit)return;edit->setText(selected);accepted=QMetaObject::invokeMethod(dialog,"accept",Qt::DirectConnection);});save->click();CHECK(accepted);CHECK(save->text()=="SAVED");
 std::cout<<"Saved path: "<<save->toolTip().toStdString()<<" expected: "<<selected.toStdString()<<"\n";DspState external,cache;CHECK(external.load(std::filesystem::path((selected+".Edsp").toStdWString()),error));CHECK(cache.load(root/"state/Stage2State.json",error));CHECK(external.centerConfigured&&external.centerSettings.values==settings.values);CHECK(cache.centerSettings.values==settings.values);CHECK(external.importedProject==initial.importedProject);
 if(argc>1)window.grab().save(QString::fromLocal8Bit(argv[1]));
 window.close();std::cout<<"PASS real SAVE dialog, Documents default, cancel unchanged, Unicode Edsp roundtrip, startup profile, confirmation, version and removed footer\n";
}
