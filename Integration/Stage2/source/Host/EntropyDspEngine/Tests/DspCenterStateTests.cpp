#include "TestSupport.h"
#include "DspState.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>
int main(int argc,char** argv){QCoreApplication app(argc,argv);QTemporaryDir tmp;auto file=std::filesystem::path(tmp.path().toStdWString())/"state.json";DspState state;state.centerConfigured=true;state.centerSettings.values[0]=0;state.centerSettings.values[1]=4;state.centerSettings.values[13]=12.5;state.centerSettings.values[14]=3.25;state.importedProject="opaque project";state.importedIni="opaque ini";std::string error;CHECK(state.save(file,error));DspState restored;CHECK(restored.load(file,error));CHECK(restored.centerConfigured && restored.centerSettings.values==state.centerSettings.values);CHECK(restored.importedProject==state.importedProject && restored.importedIni==state.importedIni);QFile f(QString::fromStdWString(file.wstring()));CHECK(f.open(QIODevice::ReadOnly));auto object=QJsonDocument::fromJson(f.readAll()).object();CHECK(object.value("mode").toString()=="CENTER_V2_C_ONLY");CHECK(!object.value("pdc_active").toBool());CHECK(!object.value("algorithmic_latency_samples").isDouble());f.close();auto settings=object.value("center_settings").toObject();settings.insert("fftSize",4.5);object.insert("center_settings",settings);CHECK(f.open(QIODevice::WriteOnly|QIODevice::Truncate));f.write(QJsonDocument(object).toJson());f.close();CHECK(!restored.load(file,error));std::cout<<"PASS Center state round-trip/opaque originals preserved/invalid FFT refused/no false zero latency/no LRC alignment\n";}
