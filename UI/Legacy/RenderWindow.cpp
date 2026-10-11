#include "RenderWindow.h"
#include <windows.h>
#include "RenderStyle.h"
#include "R10/CrossoverPage.h"
#include "R10/OutputMatrixPage.h"
#include "R10/HeadroomPage.h"
#include "R10/DeviceManagerPage.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QPushButton>
#include <QTimer>
#include <QFileDialog>

#include <QMessageBox>
#include <QTabWidget>
namespace entropy::stage {
RenderWindow::RenderWindow(entropy::ui::EngineFacade& controller):controller_(controller){
 setWindowTitle("Entropy — Render Engine · Stage 3");resize(1440,820);setMinimumSize(1160,720);entropy::host::applySourceStyle(*this);
 auto* page=new QWidget(this);setCentralWidget(page);auto* root=new QVBoxLayout(page);root->setContentsMargins(20,16,20,14);root->setSpacing(14);
 auto* header=new QHBoxLayout;auto* brandBox=new QVBoxLayout;auto* brand=new QLabel("ENTROPY",page);brand->setObjectName("brand");brandBox->addWidget(brand);auto* subtitle=new QLabel("MODULAR AUDIO PROCESSOR",page);subtitle->setObjectName("subtitle");brandBox->addWidget(subtitle);header->addLayout(brandBox);header->addSpacing(28);
 auto* titleBox=new QVBoxLayout;auto* title=new QLabel("RENDER · STAGE 3",page);title->setStyleSheet("font-size:24px;letter-spacing:4px;font-weight:600;");titleBox->addWidget(title);auto* desc=new QLabel("0.1.9-v2  ·  CROSSOVER  ·  OUTPUT MATRIX  ·  HEADROOM  ·  EGRESS",page);desc->setObjectName("subtitle");titleBox->addWidget(desc);header->addLayout(titleBox);header->addStretch();
 programShortcut_=new QPushButton("PROGRAM  •  —",page);programShortcut_->setObjectName("renderProgramShortcut");programShortcut_->setToolTip("Open Crossover — change MUSIC/CINEMA on the original page");header->addWidget(programShortcut_);QObject::connect(programShortcut_,&QPushButton::clicked,this,[this]{openModule(RenderPage::Crossover);auto* tabs=dialogs_[0]->findChild<QTabWidget*>("crossoverTabs");if(tabs)for(int i=0;i<tabs->count();i++)if(tabs->tabText(i)=="MUSIC/CINEMA"){tabs->setCurrentIndex(i);break;}});
 outputShortcut_=new QPushButton("OUTPUT  •  DISARMED",page);outputShortcut_->setObjectName("renderOutputShortcut");outputShortcut_->setToolTip("Open Devices — ARM/DISARM on the original page");header->addWidget(outputShortcut_);QObject::connect(outputShortcut_,&QPushButton::clicked,this,[this]{openModule(RenderPage::Devices);});root->addLayout(header);
 auto* group=new QGroupBox("RENDER GRAPH",page);auto* rows=new QVBoxLayout(group);rows->setContentsMargins(10,18,10,10);auto* chain=new QHBoxLayout;chain->setSpacing(5);const int stretch[]={15,29,23,19,26};
 for(unsigned i=0;i<5;i++){panels_[i]=new RenderOverview(OverviewKind(i),*snapshot_,group);chain->addWidget(panels_[i],stretch[i]);if(i){const auto target=i==4?RenderPage::Devices:RenderPage(i-1);panels_[i]->setActivated([this,target]{openModule(target);});nodes_[unsigned(target)]=panels_[i];}if(i<4){auto* arrow=new QLabel("→",group);arrow->setAlignment(Qt::AlignCenter);arrow->setFixedWidth(16);arrow->setStyleSheet("color:#20bfff;font-size:21px;font-weight:700;");chain->addWidget(arrow);}}
 rows->addLayout(chain,1);connection_=new QLabel(group);connection_->setObjectName("renderConnectionState");connection_->setStyleSheet("color:#8da9bd;font-size:11px;padding:8px 2px 0;");rows->addWidget(connection_);root->addWidget(group,1);
 latency_=new QLabel(page);latency_->setObjectName("renderStage3Latency");latency_->setStyleSheet("color:#59d6ff;font-size:11px;");root->addWidget(latency_);
 auto* footer=new QHBoxLayout;auto* hint=new QLabel("RENDER  ·  MUSIC3 / CINEMA6/8 → 16   |   Click a module to open its original controls",page);hint->setObjectName("subtitle");footer->addWidget(hint,1);auto* save=new QPushButton("SAVE AS",page);auto* load=new QPushButton("LOAD",page);footer->addWidget(save);footer->addWidget(load);root->addLayout(footer);
 QObject::connect(save,&QPushButton::clicked,this,[this]{auto path=QFileDialog::getSaveFileName(this,"Save Render project",QString(),"Entropy project (*.entropy)");if(path.isEmpty())return;uint64_t seq=0;if(!entropy::host::writeEngineProjectRequest(entropy::host::EngineProjectSave,path.toStdWString().c_str(),seq))QMessageBox::warning(this,"Save refused","Project command queue unavailable");});
 QObject::connect(load,&QPushButton::clicked,this,[this]{auto path=QFileDialog::getOpenFileName(this,"Load Render project — output stays DISARM",QString(),"Entropy project (*.entropy)");if(path.isEmpty())return;uint64_t seq=0;if(!entropy::host::writeEngineProjectRequest(entropy::host::EngineProjectLoad,path.toStdWString().c_str(),seq))QMessageBox::warning(this,"Load refused","Project command queue unavailable");});
 // Device drafts are submitted only by an explicit user action.

 auto* timer=new QTimer(this);timer->setInterval(100);QObject::connect(timer,&QTimer::timeout,this,[this]{if(isVisible()&&!isMinimized())refresh();});timer->start();refresh();
}
void RenderWindow::openModule(RenderPage kind){
 const auto i=unsigned(kind);if(dialogs_[i]){dialogs_[i]->showNormal();dialogs_[i]->raise();dialogs_[i]->activateWindow();return;}
 auto* dialog=new QDialog(this,Qt::Window|Qt::WindowCloseButtonHint|Qt::WindowMinMaxButtonsHint);dialogs_[i]=dialog;dialog->setObjectName(QString("renderModuleDialog%1").arg(i));const char* titles[]={"CROSSOVER","OUTPUT MATRIX","HEADROOM MANAGER","DEVICES"};dialog->setWindowTitle(titles[i]);dialog->resize(1100,780);auto* layout=new QVBoxLayout(dialog);QWidget* original=nullptr;
 if(kind==RenderPage::Crossover)original=entropy::host::createCrossoverView(dialog);else if(kind==RenderPage::Matrix)original=entropy::host::createOutputMatrixView(dialog);else if(kind==RenderPage::Headroom)original=entropy::host::createHeadroomView(dialog);else original=entropy::host::createDeviceManagerView(dialog);
 if(original){auto* scroll=new QScrollArea(dialog);scroll->setWidgetResizable(true);scroll->setWidget(original);layout->addWidget(scroll,1);}
 auto* close=new QPushButton("RETURN TO GRAPH",dialog);QObject::connect(close,&QPushButton::clicked,dialog,&QDialog::close);layout->addWidget(close,0,Qt::AlignRight);dialog->show();
}
void RenderWindow::refresh(){
 if(!controller_.bridge().read(*snapshot_))return;presentSelectedCrossoverProgram(*snapshot_);const auto& s=*snapshot_;
 programShortcut_->setText(s.crossoverControlAvailable?(s.crossoverCinemaProgram?"PROGRAM  •  CINEMA":"PROGRAM  •  MUSIC"):"PROGRAM  •  UNAVAILABLE");programShortcut_->setStyleSheet(s.crossoverCinemaProgram?"border-color:#aa70ff;color:#d2b3ff;":"border-color:#20bfff;color:#59d6ff;");
 outputShortcut_->setText(s.outputFaultLatched?"OUTPUT  •  FAULT":s.outputArmed?(s.outputGateClosed?"OUTPUT  •  ARMED / MUTE":"OUTPUT  •  ARMED"):"OUTPUT  •  DISARMED");outputShortcut_->setStyleSheet(s.outputFaultLatched?"border-color:#ff7070;color:#ff7070;":s.outputArmed&&!s.outputGateClosed?"border-color:#46df70;color:#46df70;":"border-color:#f0bd4c;color:#f0bd4c;");
 LARGE_INTEGER now,f;QueryPerformanceCounter(&now);QueryPerformanceFrequency(&f);auto last=controller_.lastBlockQpc();const bool fresh=controller_.connected()&&last&&uint64_t(now.QuadPart)>=last&&uint64_t(now.QuadPart)-last<uint64_t(f.QuadPart);for(auto* panel:panels_)panel->refresh(fresh);
 uint64_t xruns=0;for(unsigned i=0;i<std::min<unsigned>(s.outputDomainCount,entropy::host::engineMaximumDacDomains);i++)xruns+=s.outputDomains[i].xrunCount;
 connection_->setText(QString("%1   •   %2   •   OUTPUT %3   •   FP64 / 192 kHz   •   13 → 16   •   blocks %4   •   Graph fault flags %5   •   underruns %6").arg(fresh?"LIVE":"WAITING FOR DSP").arg(s.crossoverCinemaProgram?"CINEMA":"MUSIC").arg(s.outputArmed?"ARMED":"DISARMED").arg(controller_.blocks()).arg(controller_.faults()).arg(xruns));
 QStringList totals,details;
 for(unsigned dac=0;dac<2;dac++){const auto& b=s.stage3Latency[dac];
  totals<<(!fresh||!b.available?QString("DAC %1: unavailable").arg(dac+1):QString("DAC %1: %2–%3 ms").arg(dac+1).arg(b.minimumMs,0,'f',2).arg(b.maximumMs,0,'f',2));
  if(b.available)details<<QString("DAC %1 · FIR %2 ms + routed delay %3–%4 ms + FIFO %5 ms + ClockSync SRC %6 ms + sink buffer %7 ms + Guardian %8 ms").arg(dac+1).arg(b.firMs,0,'f',3).arg(b.delayMinMs,0,'f',3).arg(b.delayMaxMs,0,'f',3).arg(b.fifoMs,0,'f',3).arg(b.clockMs,0,'f',3).arg(b.sinkMs,0,'f',3).arg(b.guardianMs,0,'f',3);
 }
 latency_->setText("STAGE 3 LATENCY · estimated · "+totals.join("   |   "));
 latency_->setToolTip(details.join("\n")+"\nStage3 input to DAC buffer; current scene delay targets after smoothing. Matrix/Headroom/PCM24 add no lookahead. IIR frequency-dependent delay, current sink padding and physical DAC delay are unmeasured. Stage1/Stage2 excluded. 1024-frame block size is not added as a fictitious delay.");
}
}
