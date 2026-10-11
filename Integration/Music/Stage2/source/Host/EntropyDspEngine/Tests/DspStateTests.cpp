#include "DspState.h"
#include "TestSupport.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
int main(int argc,char** argv){QCoreApplication app(argc,argv);QTemporaryDir temp;std::string error;DspState state;state.importedProject=QByteArray("opaque\0R10",10);state.importedIni=QByteArray("saved\nini\n");auto path=std::filesystem::path(temp.path().toStdWString())/"state.json";CHECK(state.save(path,error));DspState loaded;CHECK(loaded.load(path,error));CHECK(loaded.importedProject==state.importedProject && loaded.importedIni==state.importedIni);CHECK(loaded.sourceDiscovery.isEmpty());QFile blocked(QString::fromStdWString((path.parent_path()/"blocked").wstring()));CHECK(blocked.open(QIODevice::WriteOnly));blocked.close();CHECK(!state.save(path.parent_path()/"blocked"/"profile.Edsp",error));CHECK(!error.empty());std::cout<<"PASS private inactive state roundtrip, opaque R10 bytes retained\n";}
