#include "DspState.h"
#include "TestSupport.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
int main(int argc,char** argv){
 QCoreApplication app(argc,argv);QTemporaryDir temp;CHECK(temp.isValid());auto root=std::filesystem::path(temp.path().toStdWString());std::string error,warning;QString selected;
 DspState state;state.importedProject=QByteArray("opaque\0R10",10);state.importedIni=QByteArray("saved\nini\n");state.alignmentActive=true;state.recoverySettings.enabled=1;state.recoverySettings.transient_strength=.72;
 auto path=root/"state.json";CHECK(state.save(path,error));DspState loaded;CHECK(loaded.load(path,error));CHECK(loaded.importedProject==state.importedProject&&loaded.importedIni==state.importedIni&&loaded.alignmentActive==true);CHECK(loaded.recoverySettings.enabled==1&&loaded.recoverySettings.transient_strength==.72);
 QFile blocked(QString::fromStdWString((root/"blocked").wstring()));CHECK(blocked.open(QIODevice::WriteOnly));blocked.close();CHECK(!state.save(root/"blocked/profile.Edsp",error));CHECK(!error.empty());
 const auto external=QString::fromStdWString((root/std::filesystem::path(L"Profil muzică.Edsp")).wstring());CHECK(state.save(std::filesystem::path(external.toStdWString()),error));CHECK(state.remember(root,external,error));
 DspState changed=state;changed.centerConfigured=true;changed.centerSettings.values[13]=7.25;changed.alignmentActive=false;CHECK(changed.save(std::filesystem::path(external.toStdWString()),error));
 CHECK(loaded.loadStartup(root,selected,warning,error));std::cout<<"Startup selected="<<selected.toStdString()<<" expected="<<external.toStdString()<<" warning="<<warning<<" center="<<loaded.centerSettings.values[13]<<" alignment="<<loaded.alignmentActive.value_or(true)<<"\n";CHECK(QFileInfo(selected).absoluteFilePath()==QFileInfo(external).absoluteFilePath()&&warning.empty()&&loaded.centerSettings.values[13]==7.25&&loaded.alignmentActive==false);
 QFile profile(external);CHECK(profile.remove());CHECK(loaded.loadStartup(root,selected,warning,error));CHECK(!warning.empty()&&loaded.importedProject==state.importedProject&&loaded.alignmentActive==true);
 CHECK(profile.open(QIODevice::WriteOnly));profile.write("broken");profile.close();CHECK(loaded.loadStartup(root,selected,warning,error));CHECK(!warning.empty()&&loaded.alignmentActive==true);
 QFile selection(QString::fromStdWString((root/"state/LastProfile.json").wstring()));CHECK(selection.open(QIODevice::WriteOnly|QIODevice::Truncate));selection.write("broken");selection.close();CHECK(loaded.loadStartup(root,selected,warning,error));CHECK(selected.isEmpty()&&!warning.empty()&&loaded.alignmentActive==true);
 QFile plain(QString::fromStdWString(path.wstring()));CHECK(plain.open(QIODevice::ReadOnly));auto object=QJsonDocument::fromJson(plain.readAll()).object();plain.close();object["lrc_alignment_active"]=1;CHECK(plain.open(QIODevice::WriteOnly|QIODevice::Truncate));plain.write(QJsonDocument(object).toJson());plain.close();CHECK(!loaded.load(path,error));CHECK(loaded.alignmentActive==true);
 object.remove("lrc_alignment_active");CHECK(plain.open(QIODevice::WriteOnly|QIODevice::Truncate));plain.write(QJsonDocument(object).toJson());plain.close();CHECK(loaded.load(path,error));CHECK(!loaded.alignmentActive.has_value());
 auto recovery=object.value("dynamic_recovery_settings").toObject();recovery.insert("wet_mix",2.0);object.insert("dynamic_recovery_settings",recovery);CHECK(plain.open(QIODevice::WriteOnly|QIODevice::Truncate));plain.write(QJsonDocument(object).toJson());plain.close();CHECK(!loaded.load(path,error));
 object.remove("dynamic_recovery_settings");CHECK(plain.open(QIODevice::WriteOnly|QIODevice::Truncate));plain.write(QJsonDocument(object).toJson());plain.close();CHECK(loaded.load(path,error));CHECK(loaded.recoverySettings.enabled==0);
 std::cout<<"PASS profile path persistence, reload actual file, missing/corrupt fallback, strict LRC flag, legacy compatibility and opaque state roundtrip\n";
}
