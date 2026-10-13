#include "DspGraphPage.h"
#include "CenterPage.h"
#include "AlignmentPage.h"
#include "RecoveryPage.h"
#include "R10/LoudnessPage.h"
#include "R10/ParametricEqPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QPushButton>
namespace entropy::stage {
namespace {
QLabel* text(const QString& value,QWidget* parent=nullptr){auto* l=new QLabel(value,parent);l->setWordWrap(true);l->setAlignment(Qt::AlignCenter);return l;}
GraphNodeFrame* card(const QString& title,const QString& detail,const QString& state,const char* accent,QWidget* parent,bool placeholder=false){
 auto* node=new GraphNodeFrame(parent);node->setProperty("graphNode",true);node->setProperty("placeholder",placeholder);node->setMinimumWidth(150);node->setStyleSheet(QString("QFrame[graphNode=\"true\"] {background:#091e29;border:1px %2 #264a5d;border-left:3px solid %1;border-radius:6px;} QLabel {border:0;}").arg(accent,placeholder?"dashed":"solid"));
 auto* v=new QVBoxLayout(node);v->setContentsMargins(10,8,10,8);v->setSpacing(5);auto* heading=text(title);heading->setObjectName("stageTitle");v->addWidget(heading);v->addWidget(text(detail),1);auto* stateLabel=text(state);stateLabel->setStyleSheet(QString("color:%1;").arg(placeholder?"#617887":"#f0bd4c"));v->addWidget(stateLabel);return node;
}
QLabel* arrow(const char* color){auto* l=text("→");l->setFixedWidth(28);l->setStyleSheet(QString("font-size:24px;font-weight:700;color:%1;").arg(color));return l;}
}
DspGraphPage::DspGraphPage(const std::filesystem::path&,QWidget* parent):QWidget(parent){
 setObjectName("dspGraphPage");auto* v=new QVBoxLayout(this);v->setContentsMargins(0,0,0,0);v->setSpacing(10);
 auto* head=new QGroupBox("DSP GRAPH",this);auto* h=new QHBoxLayout(head);h->addWidget(text("MUSIC   •   2 IN → 3 OUT   •   FP64 / 192 kHz"),1);status_=text("WAITING FOR STAGEBUS");status_->setObjectName("graphConnectionStatus");h->addWidget(status_);v->addWidget(head);
 auto* scroll=new QScrollArea(this);scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);auto* canvas=new QWidget(scroll);auto* row=new QHBoxLayout(canvas);row->setContentsMargins(8,4,8,4);row->setSpacing(8);
 auto* incoming=new QWidget(canvas);auto* i=new QVBoxLayout(incoming);i->setContentsMargins(0,0,0,0);incoming->setFixedWidth(158);i->addWidget(card("MUSIC L / R","Raw programme mix\nMUSIC + AUX","STAGEBUS IN","#20bfff",incoming),1);row->addWidget(incoming);row->addWidget(arrow("#20bfff"));
 input_=new MatrixGraphNode("MX IN",2,2,QColor("#46df70"),canvas);input_->setObjectName("matrixIn");row->addWidget(input_);row->addWidget(arrow("#46df70"));
 auto* routes=new QWidget(canvas);routes->setMinimumWidth(550);auto* routesLayout=new QVBoxLayout(routes);routesLayout->setContentsMargins(0,0,0,0);routesLayout->setSpacing(12);
 auto* chain=new QGroupBox("MUSIC L / R — MODULAR PATH",routes);auto* chainLayout=new QHBoxLayout(chain);chainLayout->setSpacing(6);
 modules_[0]=card("LOUDNESS","Stereo L/R\nOriginal module interface","BYPASS / INACTIVE","#20bfff",chain);modules_[1]=card("PARAMETRIC EQ","Stereo L/R\nOriginal module interface","BYPASS / INACTIVE","#20bfff",chain);modules_[3]=card("DYNAMIC RECOVERY","Stereo L/R linked\nTransient + upward expansion","BYPASS / EXACT WIRE","#20bfff",chain);
 chainLayout->addWidget(modules_[0],1);chainLayout->addWidget(arrow("#46df70"));chainLayout->addWidget(modules_[1],1);chainLayout->addWidget(arrow("#46df70"));chainLayout->addWidget(modules_[3],1);routesLayout->addWidget(chain,2);
 auto* branch=new QGroupBox("CENTER RAW TAP",routes);auto* branchLayout=new QHBoxLayout(branch);modules_[2]=card("CENTER EXTRACTOR V2","L/R IN → C OUT only\nL/R remain on their own path","BYPASS / C = +0.0","#20bfff",branch);branchLayout->addWidget(modules_[2],1);modules_[4]=card("LRC ALIGNMENT","MUSIC L / R / CX\nStatic integer delay\nMUSIC only","BYPASS EXACT","#46df70",branch);branchLayout->addWidget(modules_[4],1);routesLayout->addWidget(branch,2);
 row->addWidget(routes,1);row->addWidget(arrow("#20bfff"));
 output_=new MatrixGraphNode("MX OUT",3,3,QColor("#41c8ff"),canvas);output_->setObjectName("matrixOut");row->addWidget(output_);row->addWidget(arrow("#20bfff"));auto* outgoing=card("STAGEBUS OUT","MUSIC L / R\nMUSIC_CX\n\n3 semantic channels","FP64 / 192 kHz","#20bfff",canvas);outgoing->setFixedWidth(150);outgoing->setCursor(Qt::ArrowCursor);row->addWidget(outgoing);scroll->setWidget(canvas);v->addWidget(scroll,1);
 auto* audioMode=text("BYPASS_TRANSPARENT   •   L/R unchanged   •   C = +0.0   •   Algorithmic latency: 0 samples");audioMode->setObjectName("stageAudioMode");v->addWidget(audioMode);
 modules_[1]->findChildren<QLabel*>().last()->setObjectName("eqNodeState");
 modules_[3]->findChildren<QLabel*>().last()->setObjectName("recoveryNodeState");
 for(unsigned n=0;n<modules_.size();n++){modules_[n]->setObjectName(QString("moduleNode%1").arg(n));modules_[n]->setActivated([this,n]{openModule(static_cast<DspModuleKind>(n));});}
}
void DspGraphPage::refresh(const DspUiSnapshot& s,uint64_t now){
 if(s.diag.center.loaded){
  for(auto* label:modules_[2]->findChildren<QLabel*>())if(label->text().startsWith("BYPASS / C")||label->objectName()=="centerNodeState"){label->setObjectName("centerNodeState");label->setText(s.diag.center.settings.values[15]>0?"ACTIVE / FP64 / C ONLY":"DISABLED / C = +0.0");}
  if(auto* label=findChild<QLabel*>("stageAudioMode"))label->setText("CENTER V2   •   L/R direct   •   MUSIC_CX only   •   L/R/C alignment: native DLL");
 }else if(auto* label=findChild<QLabel*>("centerNodeState")){label->setText("CENTER NOT LOADED");if(auto* mode=findChild<QLabel*>("stageAudioMode"))mode->setText("CENTER NOT LOADED   •   Audio stopped");}
 if(dialogs_[2]&&dialogs_[2]->isVisible()&&!dialogs_[2]->isMinimized())refreshStage2CenterView(dialogs_[2]->findChild<QWidget*>("center.page"),s);
 for(auto* label:modules_[4]->findChildren<QLabel*>())if(label->text()=="BYPASS EXACT"||label->objectName()=="alignmentNodeState"){label->setObjectName("alignmentNodeState");label->setText(alignmentStatus(s.diag.alignment));}
 if(dialogs_[4]&&dialogs_[4]->isVisible()&&!dialogs_[4]->isMinimized())refreshAlignmentView(dialogs_[4]->findChild<QWidget*>("alignment.page"),s);
 if(s.diag.alignment.loaded)if(auto* label=findChild<QLabel*>("stageAudioMode"))label->setText(s.diag.alignment.mode?"CENTER V2   •   LRC STATIC COMPENSATION":"CENTER V2   •   LRC BYPASS EXACT");
 if(auto* label=modules_[1]->findChild<QLabel*>("eqNodeState"))label->setText(s.diag.eq.loaded?(s.diag.eq.settings.mode?"ACTIVE / FP64 / STEREO":"BYPASS / EXACT WIRE"):"BYPASS / INACTIVE");
 if(auto* label=modules_[3]->findChild<QLabel*>("recoveryNodeState"))label->setText(s.diag.recovery.loaded?(s.diag.recovery.active?"ACTIVE / FP64 / LINKED":"BYPASS / EXACT WIRE"):"MODULE UNAVAILABLE");
 if(dialogs_[3]&&dialogs_[3]->isVisible()&&!dialogs_[3]->isMinimized())refreshRecoveryView(dialogs_[3]->findChild<QWidget*>("recovery.page"),s);
 const bool receiving=s.state==DspStateCode::Running;auto masks=liveSignalMasks(s.diag.signal,now,s.outputSession.qpc_frequency,receiving);
 input_->setLaneActivity(masks.first,masks.first,0,receiving);output_->setLaneActivity(masks.second,masks.second,0,receiving);
 status_->setText(receiving?(masks.first?"● CONNECTED / SIGNAL ACTIVE":"● CONNECTED / SILENCE"):s.state==DspStateCode::Stopped?"STOPPED":"WAITING FOR STAGEBUS");status_->setStyleSheet(receiving?"color:#43e878;font-weight:700;":"color:#f0bd4c;font-weight:700;");
}
void DspGraphPage::openModule(DspModuleKind kind){
 const auto index=static_cast<unsigned>(kind);if(dialogs_[index]){dialogs_[index]->showNormal();dialogs_[index]->raise();dialogs_[index]->activateWindow();return;}
 auto* dialog=new QDialog(window(),Qt::Window|Qt::WindowCloseButtonHint|Qt::WindowMinMaxButtonsHint);dialogs_[index]=dialog;dialog->setObjectName(QString("moduleDialog%1").arg(index));const char* titles[]={"LOUDNESS","PARAMETRIC EQ","CENTER EXTRACTOR V2 — LR IN / C OUT","DYNAMIC RECOVERY — MUSIC L/R","LRC ALIGNMENT — MUSIC L/R/CX"};dialog->setWindowTitle(titles[index]);dialog->resize(std::max(800,qRound(window()->width()*.72)),std::max(600,qRound(window()->height()*.72)));dialog->move(window()->geometry().center()-QPoint(dialog->width()/2,dialog->height()/2));auto* layout=new QVBoxLayout(dialog);
 CenterSettings requested;const bool liveCenter=kind==DspModuleKind::Center&&controller_&&controller_->requestedCenterSettings(requested);
 layout->addWidget(text(kind==DspModuleKind::Alignment?"MUSIC L / R / CX   •   FP64 / 192 kHz   •   Manual APPLY":kind==DspModuleKind::Recovery?"DYNAMIC RECOVERY   •   FP64 / 192 kHz   •   STEREO LINKED   •   0 SAMPLES LATENCY":kind==DspModuleKind::Eq&&controller_?"PARAMETRIC EQ SLOT 2   •   FP64 / 192 kHz   •   MUSIC L/R ONLY":liveCenter?"CENTER V2   •   FP64 / 192 kHz   •   MUSIC + AUX L/R IN → MUSIC_CX OUT":"Original module interface   •   DSP inactive in transparent bypass"));
 QWidget* original=nullptr;if(kind==DspModuleKind::Alignment)original=createAlignmentView(dialog,controller_);else if(kind==DspModuleKind::Center)original=createStage2CenterView(dialog,controller_);else if(kind==DspModuleKind::Loudness)original=entropy::host::createLoudnessView(dialog);else if(kind==DspModuleKind::Eq)original=entropy::host::createParametricEqView(dialog,controller_);else if(kind==DspModuleKind::Recovery)original=createRecoveryView(dialog,controller_);
 if(original){original->setEnabled(liveCenter||kind==DspModuleKind::Alignment||(kind==DspModuleKind::Eq&&controller_)||kind==DspModuleKind::Recovery);auto* scroll=new QScrollArea(dialog);scroll->setWidgetResizable(true);scroll->setWidget(original);layout->addWidget(scroll,1);}else layout->addStretch();
 if(kind!=DspModuleKind::Center&&kind!=DspModuleKind::Alignment&&kind!=DspModuleKind::Eq){auto* close=new QPushButton("RETURN TO GRAPH",dialog);QObject::connect(close,&QPushButton::clicked,dialog,&QDialog::close);layout->addWidget(close,0,Qt::AlignRight);}dialog->show();
}
void DspGraphPage::resetProfileViews(){
 // Destroy pending UI debounce timers and recreate controls from the loaded
 // controller settings on next open; stale editors must not overwrite LOAD.
 for(unsigned index:{1u,2u,3u,4u}){delete dialogs_[index];dialogs_[index]=nullptr;}
}
}
