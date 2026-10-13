#include "DspWindow.h"
#include "DspGraphPage.h"
#include "DspState.h"
#include "DspStyle.h"
#include "DspVersion.h"
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QTimer>
#include <QGuiApplication>
#include <QScreen>
#include <memory>
namespace entropy::stage {
DspWindow::DspWindow(DspController& controller,const std::filesystem::path& root,bool startAutomatic){
 setWindowTitle(QString("DSP Engine · %1").arg(dspVersion));resize(1360,960);if(auto* screen=QGuiApplication::primaryScreen())resize(std::min(width(),screen->availableGeometry().width()-40),std::min(height(),screen->availableGeometry().height()-60));entropy::host::applySourceStyle(*this);
 auto* page=new QWidget(this);setCentralWidget(page);auto* layout=new QVBoxLayout(page);layout->setContentsMargins(16,12,16,12);
 auto* top=new QHBoxLayout;auto* brand=new QLabel(QString("DSP Engine · %1").arg(dspVersion),page);brand->setObjectName("brand");top->addWidget(brand,1);auto* diagnostics=new QPushButton("DIAGNOSTICS",page);auto* load=new QPushButton("LOAD",page);load->setObjectName("dspLoad");load->setMinimumWidth(110);auto* save=new QPushButton("SAVE",page);save->setObjectName("dspSave");save->setMinimumWidth(110);top->addWidget(diagnostics);top->addWidget(load);top->addWidget(save);layout->addLayout(top);
 auto* profileStatus=new QLabel(page);profileStatus->setObjectName("dspProfileStatus");profileStatus->setWordWrap(true);profileStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);layout->addWidget(profileStatus);
 auto* graph=new DspGraphPage(root,page);graph->bindController(controller);layout->addWidget(graph,1);
 auto* diag=new QLabel(page);diag->setObjectName("dspDiagnostics");diag->setWordWrap(true);diag->setTextInteractionFlags(Qt::TextSelectableByMouse);diag->hide();layout->addWidget(diag);
 connect(diagnostics,&QPushButton::clicked,this,[diag]{diag->setVisible(!diag->isVisible());});
 auto state=std::make_shared<DspState>();std::string error,warning;QString selected;const bool startupOk=state->loadStartup(root,selected,warning,error);
 auto lastSave=std::make_shared<QString>(selected.isEmpty()?QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath("DSP Engine.Edsp"):selected);
 auto pendingAlignment=std::make_shared<std::optional<bool>>(state->alignmentActive);
 auto showProfile=[profileStatus,save,load](const QString& file,const QString& message=QString()){
  save->setToolTip(file);load->setToolTip(file);profileStatus->setToolTip(file);
  profileStatus->setText(message.isEmpty()?(file.isEmpty()?"Profile: startup cache / defaults":QString("Profile: %1").arg(QFileInfo(file).fileName())):message);
 };
 showProfile(selected,!startupOk?QString::fromStdString(error):QString::fromStdString(warning));
 connect(save,&QPushButton::clicked,this,[this,state,root,&controller,save,lastSave,pendingAlignment,showProfile](){
  QFileDialog dialog(this,"Save DSP profile",*lastSave,"DSP Engine profile (*.Edsp)");
  dialog.setObjectName("dspSaveDialog");dialog.setAcceptMode(QFileDialog::AcceptSave);dialog.setFileMode(QFileDialog::AnyFile);dialog.setDefaultSuffix("Edsp");
  if(dialog.exec()!=QDialog::Accepted || dialog.selectedFiles().isEmpty())return;
  auto file=dialog.selectedFiles().first();if(!file.endsWith(".Edsp",Qt::CaseInsensitive)){
   file+=".Edsp";
   if(QFileInfo::exists(file) && QMessageBox::question(this,"Replace profile?",QString("Replace %1?").arg(file),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
  }
  DspState next=*state;next.centerConfigured=controller.requestedCenterSettings(next.centerSettings);controller.requestedEqSettings(next.eqSettings);controller.requestedRecoverySettings(next.recoverySettings);next.sourceDiscovery.clear();std::string e;
  const auto current=controller.snapshot();if(pendingAlignment->has_value())next.alignmentActive=**pendingAlignment;else if(current.diag.alignment.loaded)next.alignmentActive=current.diag.alignment.mode!=0;
  if(!next.save(std::filesystem::path(file.toStdWString()),e)){QMessageBox::warning(this,"Save failed",QString::fromStdString(e));return;}
  *state=next;*lastSave=file;showProfile(file);
  if(!next.remember(root,file,e)){QMessageBox::warning(this,"Profile saved",QString("Saved to %1\nStartup profile could not be updated: %2").arg(file,QString::fromStdString(e)));return;}
  save->setText("SAVED");QTimer::singleShot(2000,save,[save]{save->setText("SAVE");});
 });
 connect(load,&QPushButton::clicked,this,[this,state,root,&controller,load,lastSave,pendingAlignment,showProfile,graph](){
  QFileDialog dialog(this,"Load DSP profile",*lastSave,"DSP Engine profile (*.Edsp)");dialog.setObjectName("dspLoadDialog");dialog.setAcceptMode(QFileDialog::AcceptOpen);dialog.setFileMode(QFileDialog::ExistingFile);
  if(dialog.exec()!=QDialog::Accepted||dialog.selectedFiles().isEmpty())return;
  const auto file=QFileInfo(dialog.selectedFiles().first()).absoluteFilePath();DspState next;std::string e;
  if(!next.load(std::filesystem::path(file.toStdWString()),e)){QMessageBox::warning(this,"Load failed",QString::fromStdString(e));return;}
  if(next.recoverySettings.enabled&&!controller.recoveryConfigured()){QMessageBox::warning(this,"Dynamic Recovery unavailable","This profile enables Dynamic Recovery, but EntropyDynamicRecovery.dll is unavailable. The current profile was retained.");return;}
  CenterSettings currentCenter;EqSettings currentEq{};
  if(!controller.requestedCenterSettings(currentCenter)||!controller.requestedEqSettings(currentEq)){QMessageBox::warning(this,"Load failed","DSP controls are unavailable; the current profile was retained.");return;}
  graph->resetProfileViews();uint64_t serial=0;
  // State validation and configured-controller checks precede both requests.
  // Existing mailboxes apply settings on the DSP worker's safe boundary.
  controller.requestCenter(next.centerSettings);controller.requestEq(next.eqSettings,serial);
  if(controller.recoveryConfigured())controller.requestRecovery(next.recoverySettings,serial);
  *state=next;*lastSave=file;*pendingAlignment=next.alignmentActive;showProfile(file);
  if(!next.remember(root,file,e))QMessageBox::warning(this,"Profile loaded",QString("Loaded %1\nStartup profile could not be updated: %2").arg(file,QString::fromStdString(e)));
  load->setText("LOADED");QTimer::singleShot(2000,load,[load]{load->setText("LOAD");});
 });
 if(startAutomatic && !controller.startAutomatic(root,error)){diag->setText(QString::fromStdString(error));diag->show();}
 auto* timer=new QTimer(this);timer->setInterval(100);connect(timer,&QTimer::timeout,this,[this,&controller,diag,graph,pendingAlignment](){
  // APPLY needs native latency and the acknowledged Center revision. Retry
  // through the existing control API, including while the UI is minimized.
  if(pendingAlignment->has_value()&&controller.requestAlignment(**pendingAlignment))pendingAlignment->reset();
  if(!isVisible() || isMinimized())return;auto s=controller.snapshot();LARGE_INTEGER now;QueryPerformanceCounter(&now);graph->refresh(s,now.QuadPart);
  const char* states[]={"STOPPED","WAITING DOWNSTREAM","CONNECTED","FAULT — WAITING FOR FRESH SESSION","WAITING FOR STAGEBUS"};
  if(!diag->isVisible())return;const auto& d=s.diag;const double factor=s.outputSession.qpc_frequency?1e6/s.outputSession.qpc_frequency:0;
  diag->setText(QString::fromUtf8(states[static_cast<unsigned>(s.state)])+"   "+QString::fromUtf8(s.error)+"\n"+QString("Frames IN / OUT: %1 / %2   •   Blocks IN / OUT: %3 / %4\nEpoch / sequence / first frame / revision: %5 / %6 / %7 / %8\nInvalid / missing / duplicates / faults: %9 / %10 / %11 / %12\nExecutor / traversal / source queue: %13 / %14 / %15 µs   •   Capture drops: %16").arg(d.framesIn).arg(d.framesOut).arg(d.blocksIn).arg(d.blocksOut).arg(d.lastHeader.stream_epoch).arg(d.lastHeader.block_sequence).arg(d.lastHeader.first_frame).arg(d.lastHeader.control_revision).arg(d.invalid).arg(d.missing).arg(d.duplicates).arg(d.faults).arg(d.executorQpc*factor,0,'f',1).arg(d.traversalQpc*factor,0,'f',1).arg(d.queueQpc*factor,0,'f',1).arg(d.captureDrops));
 });timer->start();
}
}
