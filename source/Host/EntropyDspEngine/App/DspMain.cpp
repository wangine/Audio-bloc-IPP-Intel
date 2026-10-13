#include "DspController.h"
#include "DspWindow.h"
#include "DspPackage.h"
#include "DspState.h"
#include <QCoreApplication>
#include <QApplication>
#include <QTimer>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QSaveFile>
#include <iostream>
#include <filesystem>
#include <windows.h>
#include <memory>
using namespace entropy::stage;
int main(int argc,char** argv){
 bool headless=false;for(int i=1;i<argc;i++)if(std::string(argv[i])=="--headless"||std::string(argv[i])=="--verify-only")headless=true;
 std::unique_ptr<QCoreApplication> app;if(headless)app=std::make_unique<QCoreApplication>(argc,argv);else app=std::make_unique<QApplication>(argc,argv);
 auto args=app->arguments();auto value=[&](const char* name){auto i=args.indexOf(name);return i>=0 && i+1<args.size()?args[i+1]:QString();};
 auto root=value("--runtime-root");if(root.isEmpty())root=QCoreApplication::applicationDirPath();
 auto source=value("--source-session"),stop=value("--stop-file"),diagnostic=value("--diagnostic-file");
 std::string error;const auto binaryRoot=std::filesystem::path(QCoreApplication::applicationDirPath().toStdWString());bool testHarness=false;
#ifdef ENTROPY_TEST_ENGINE
 testHarness=headless&&args.contains("--test-harness")&&!std::filesystem::exists(binaryRoot/"package-manifest.json");
#endif
 if(!testHarness && !verifyDspPackage(binaryRoot,error,true,true,true)){if(headless)std::cerr<<error<<"\n";else QMessageBox::critical(nullptr,"Stage2 package rejected",QString::fromStdString(error));return 2;}
 if(args.contains("--verify-only"))return 0;
 DspController controller;
 DspState startup;QString selectedProfile;std::string startupWarning;
 if(!startup.loadStartup(std::filesystem::path(root.toStdWString()),selectedProfile,startupWarning,error)){if(headless)std::cerr<<error<<"\n";else QMessageBox::critical(nullptr,"Startup profile rejected",QString::fromStdString(error));return 2;}
 if(headless&&!startupWarning.empty())std::cerr<<startupWarning<<"\n";
 if(!testHarness || args.contains("--center-native")){
  if(!controller.configureCenter(binaryRoot/"modules",startup.centerSettings,error)){
   if(headless)std::cerr<<error<<"\n";else QMessageBox::critical(nullptr,"Center configuration rejected",QString::fromStdString(error));return 2;
  }
 }
 if(!testHarness||args.contains("--eq-native")){
  if(!controller.configureEq(binaryRoot/"modules",startup.eqSettings,error)){if(headless)std::cerr<<error<<"\n";else QMessageBox::critical(nullptr,"EQ configuration rejected",QString::fromStdString(error));return 2;}
 }
 if((!testHarness||args.contains("--lrc-native"))&&!controller.configureAligner(binaryRoot/"modules",error)){if(headless)std::cerr<<error<<"\n";else QMessageBox::critical(nullptr,"LRC configuration rejected",QString::fromStdString(error));return 2;}
 if(!headless){DspWindow window(controller,std::filesystem::path(root.toStdWString()));window.show();if(args.contains("--smoke-test"))QTimer::singleShot(400,app.get(),&QCoreApplication::quit);auto result=app->exec();controller.stop();return result;}
 const bool automatic=args.contains("--automatic");
 if(source.isEmpty() && !automatic){std::cerr<<"Headless fixture requires --automatic or explicit --source-session\n";return 2;}
 auto capture=value("--capture-file");if(!capture.isEmpty()){auto count=value("--capture-blocks").toULongLong();if(!controller.startCapture(std::filesystem::path(capture.toStdWString()),count,error)){std::cerr<<error<<"\n";return 2;}}
 auto endpoint=value("--link-endpoint");
 if(!(automatic?controller.startAutomatic(std::filesystem::path(root.toStdWString()),error,endpoint.isEmpty()?entropy::publish::sourceEndpointName():endpoint.toStdWString()):controller.start(std::filesystem::path(root.toStdWString()),std::filesystem::path(source.toStdWString()),error))){std::cerr<<error<<"\n";return 2;}
 int code=0;
 auto writeDiagnostic=[&](const DspUiSnapshot& s,int status){
  if(diagnostic.isEmpty())return true;
  const auto& d=s.diag;auto micros=[&](uint64_t q){return s.outputSession.qpc_frequency?double(q)*1e6/s.outputSession.qpc_frequency:0;};
  QJsonObject report{{"blocks_in",static_cast<qint64>(d.blocksIn)},{"blocks_out",static_cast<qint64>(d.blocksOut)},{"frames_in",static_cast<qint64>(d.framesIn)},{"frames_out",static_cast<qint64>(d.framesOut)},{"invalid",static_cast<qint64>(d.invalid)},{"missing",static_cast<qint64>(d.missing)},{"duplicates",static_cast<qint64>(d.duplicates)},{"faults",static_cast<qint64>(d.faults)},{"full_observations",static_cast<qint64>(d.full)},{"deadline_faults",static_cast<qint64>(d.deadlines)},{"executor_max_us",micros(d.maxExecutorQpc)},{"traversal_max_us",micros(d.maxTraversalQpc)},{"queue_max_us",micros(d.maxQueueQpc)},{"exit_code",status}};
  report.insert("center_loaded",bool(d.center.loaded));report.insert("center_latency_valid",bool(d.center.latencyValid));report.insert("music_l_latency_samples",static_cast<qint64>(d.alignment.detected[0]+d.alignment.current[0]));report.insert("music_r_latency_samples",static_cast<qint64>(d.alignment.detected[1]+d.alignment.current[1]));report.insert("input_channels",2);report.insert("output_channels",3);report.insert("lrc_alignment_active",bool(d.alignment.mode));report.insert("lrc_loaded",bool(d.alignment.loaded));report.insert("lrc_latency_valid",bool(d.alignment.valid));report.insert("lrc_local_generation",static_cast<qint64>(d.alignment.generation));report.insert("lrc_boundary_frame",static_cast<qint64>(d.alignment.boundaryFrame));report.insert("lrc_command_serial",static_cast<qint64>(d.alignment.commandSerial));QJsonArray delays,detected;for(unsigned i=0;i<3;i++){delays.append(static_cast<qint64>(d.alignment.current[i]));detected.append(static_cast<qint64>(d.alignment.detected[i]));}report.insert("lrc_current_samples",delays);report.insert("lrc_detected_samples",detected);
  report.insert("music_cx_latency_samples",d.center.loaded?(d.center.latencyValid?QJsonValue(static_cast<int>(d.center.telemetry.latency_samples+d.alignment.current[2])):QJsonValue()):QJsonValue(0));report.insert("center_settings_revision",static_cast<qint64>(d.center.revision));
  const char* categories[]={"executor","traversal","queue"};const char* percent[]={"p50","p95","p99"};for(unsigned c=0;c<3;c++)for(unsigned p=0;p<3;p++)report.insert(QString("%1_%2_us").arg(categories[c],percent[p]),static_cast<qint64>(d.latencyPercentilesUs[c][p]));report.insert("internal_percentile_method","1 us fixed-bin upper bounds; >=2048 us overflow uses measured maximum");report.insert("capture_drops",static_cast<qint64>(d.captureDrops));
  report.insert("monitor_schedule_rebases",static_cast<qint64>(d.monitorScheduleRebases));
  report.insert("source_session_id",QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(s.outputSession.session_id),16).toHex()));
  report.insert("state",static_cast<qint64>(s.state));report.insert("error",QString::fromUtf8(s.error));
  report.insert("snapshot_tick_ms",static_cast<qint64>(GetTickCount64()));
  report.insert("publish_drops",static_cast<qint64>(d.publishDrops));
  QSaveFile file(diagnostic);return file.open(QIODevice::WriteOnly)&&file.write(QJsonDocument(report).toJson())>=0&&file.commit();
 };
 uint64_t lastDiagnostic=0;
 auto pendingAlignment=startup.alignmentActive;
 while(true){
  if(!stop.isEmpty() && std::filesystem::exists(std::filesystem::path(stop.toStdWString())))break;
  auto snapshot=controller.snapshot();
  if(pendingAlignment.has_value()&&controller.requestAlignment(*pendingAlignment))pendingAlignment.reset();
  if(!automatic && snapshot.state==DspStateCode::Fault){code=2;break;}
  auto now=GetTickCount64();if(now-lastDiagnostic>=200){if(!writeDiagnostic(snapshot,code)){code=3;break;}lastDiagnostic=now;}
  Sleep(10);
 }
 auto s=controller.snapshot();controller.stop();if(controller.snapshot().state==DspStateCode::Fault)code=2;
 if(!writeDiagnostic(s,code))return 3;
 return code;
}
