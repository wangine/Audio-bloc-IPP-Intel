#include "SourceController.h"
#include "TestSupport.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QProcess>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <fstream>
using namespace entropy::host;
int main(int argc,char**argv){
 QCoreApplication app(argc,argv);if(argc!=3)return 2;std::string error;QTemporaryDir tmp;
 auto root=std::filesystem::path(tmp.path().toStdWString());SourceController source;
 CHECK(source.start(TEST_MODULE_ROOT,root/"source",error,true));
 SourceCommand mode;mode.type=SourceCommand::BusMode;mode.value=1;CHECK(source.command(mode));
 auto deadline=GetTickCount64()+2000;while(!source.snapshot().external){CHECK(GetTickCount64()<deadline);Sleep(1);}
 QProcess dsp;dsp.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* a){a->flags|=CREATE_NO_WINDOW;});
 auto stop=root/"stop-dsp",report=root/"dsp.json",discovery=root/"state/StageBusSession.json";
 dsp.start(QString::fromLocal8Bit(argv[1]),QStringList{"--headless","--runtime-root",tmp.path(),"--source-session",QString::fromStdWString((root/"source/StageBusSession.json").wstring()),"--stop-file",QString::fromStdWString(stop.wstring()),"--diagnostic-file",QString::fromStdWString(report.wstring())});
 CHECK(dsp.waitForStarted(3000));deadline=GetTickCount64()+5000;
 while(!std::filesystem::exists(discovery)){if(dsp.waitForFinished(1)){std::cerr<<dsp.readAllStandardError().toStdString();return 1;}CHECK(GetTickCount64()<deadline);Sleep(1);}
 QFile f(QString::fromStdWString(discovery.wstring()));CHECK(f.open(QIODevice::ReadOnly));auto json=QJsonDocument::fromJson(f.readAll()).object();
 auto bytes=QByteArray::fromBase64(json.value("session_header_base64").toString().toLatin1());CHECK(bytes.size()==256);
 StageBusSessionHeader out{};std::memcpy(&out,bytes.data(),256);CHECK(out.layout_id==LRC_CINEMA11_V1);
 StageBusWin32 consumer;CHECK(consumer.open(out,error));Planes<11> audio;StageBusBlockHeader block{};
 for(uint64_t seq=1;seq<=128;++seq){
  consumer.heartbeat(qpc());auto r=request(out,seq);r.program.hardware_armed=seq%2;
  CHECK(consumer.trySendRequest(r)==StageBusIoResult::Ok);deadline=GetTickCount64()+2000;StageBusIoResult io;
  while((io=consumer.tryConsume(block,audio.out.data()))==StageBusIoResult::Empty){CHECK(GetTickCount64()<deadline);consumer.heartbeat(qpc());Sleep(1);}
  CHECK(io==StageBusIoResult::Ok && block.block_sequence==seq);CHECK(source.snapshot().faults==0);
 }
 CHECK(source.snapshot().sequence==128 && source.snapshot().demandSourceMask==7);
 std::ofstream(stop).put('1');CHECK(dsp.waitForFinished(5000));
 if(dsp.exitCode()!=0)std::cerr<<dsp.readAllStandardError().toStdString();CHECK(dsp.exitCode()==0);
 std::filesystem::copy_file(report,std::filesystem::path(QString::fromLocal8Bit(argv[2]).toStdWString()),std::filesystem::copy_options::overwrite_existing);
 consumer.close();source.stop();
 std::cout<<"PASS current unmodified Stage2 executable: 128 consecutive Source10 -> DSP11 blocks, zero Source faults; explicit V1 session attachment\n";
}
