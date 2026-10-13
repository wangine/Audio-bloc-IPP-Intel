#include "RecoveryPage.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <cmath>

namespace entropy::stage {
QWidget* createRecoveryView(QWidget* parent,DspController* controller){
 auto* page=new QWidget(parent);page->setObjectName("recovery.page");
 auto* layout=new QVBoxLayout(page);layout->setSpacing(14);
 auto* status=new QLabel("DYNAMIC RECOVERY — WAITING FOR WORKER",page);status->setObjectName("recovery.status");status->setStyleSheet("font-size:17px;font-weight:700;color:#f0bd4c;");layout->addWidget(status);
 auto* note=new QLabel("Stereo linked full-band gain on MUSIC L/R. Center receives its existing raw tap. Zero algorithmic transport latency. Positive boost can exceed 0 dBFS.",page);note->setWordWrap(true);layout->addWidget(note);
 EntropyDynamicRecoveryConfigV1 initial=defaultRecoverySettings();
 const bool available=controller&&controller->requestedRecoverySettings(initial);
 auto submit=[controller,status](auto change){
  if(!controller)return;EntropyDynamicRecoveryConfigV1 settings{};
  if(!controller->requestedRecoverySettings(settings))return;
  change(settings);uint64_t revision=0;
  if(!controller->requestRecovery(settings,revision))status->setText("CONTROL REJECTED — CHECK PARAMETERS");
  else status->setText(QString("APPLYING AT BLOCK BOUNDARY · REVISION %1").arg(revision));
 };
 auto* enable=new QPushButton("ENABLE DYNAMIC RECOVERY",page);enable->setObjectName("recovery.enabled");enable->setCheckable(true);enable->setProperty("positiveToggle",true);enable->setChecked(initial.enabled);enable->setEnabled(available);layout->addWidget(enable);
 QObject::connect(enable,&QPushButton::toggled,page,[submit](bool value){submit([value](auto& c){c.enabled=value;});});
 auto* transient=new QGroupBox("TRANSIENT RECOVERY",page);auto* tgrid=new QGridLayout(transient);layout->addWidget(transient);
 auto* expansion=new QGroupBox("UPWARD EXPANSION · OPTIONAL",page);auto* egrid=new QGridLayout(expansion);layout->addWidget(expansion);
 auto add=[&](QGridLayout* grid,int row,const char* name,const char* id,double lo,double hi,double value,double EntropyDynamicRecoveryConfigV1::*field,const char* unit){
  auto* title=new QLabel(name,page);auto* slider=new QSlider(Qt::Horizontal,page);slider->setObjectName(QString("recovery.%1").arg(id));slider->setRange(int(std::lround(lo*100)),int(std::lround(hi*100)));slider->setValue(int(std::lround(value*100)));slider->setEnabled(available);
  auto* readout=new QLabel(page);readout->setMinimumWidth(100);auto update=[readout,unit](int raw){readout->setText(QString::number(raw/100.0,'f',2)+" "+unit);};update(slider->value());
  grid->addWidget(title,row,0);grid->addWidget(slider,row,1);grid->addWidget(readout,row,2);grid->setColumnStretch(1,1);
  QObject::connect(slider,&QSlider::valueChanged,page,[submit,update,field](int raw){update(raw);submit([field,raw](auto& c){c.*field=raw/100.0;});});
 };
 add(tgrid,0,"Strength","strength",0,1,initial.transient_strength,&EntropyDynamicRecoveryConfigV1::transient_strength,"");
 add(tgrid,1,"Onset threshold","onset",0,12,initial.onset_threshold_db,&EntropyDynamicRecoveryConfigV1::onset_threshold_db,"dB");
 add(tgrid,2,"Attack boost cap","attackBoost",0,6,initial.max_attack_boost_db,&EntropyDynamicRecoveryConfigV1::max_attack_boost_db,"dB");
 auto* expandEnable=new QPushButton("ENABLE UPWARD EXPANSION",page);expandEnable->setObjectName("recovery.expansionEnabled");expandEnable->setCheckable(true);expandEnable->setProperty("positiveToggle",true);expandEnable->setChecked(initial.expansion_enabled);expandEnable->setEnabled(available);egrid->addWidget(expandEnable,0,0,1,3);
 QObject::connect(expandEnable,&QPushButton::toggled,page,[submit](bool value){submit([value](auto& c){c.expansion_enabled=value;});});
 add(egrid,1,"Ratio","ratio",1,1.5,initial.expansion_ratio,&EntropyDynamicRecoveryConfigV1::expansion_ratio,"×");
 add(egrid,2,"Threshold","threshold",-36,-6,initial.expansion_threshold_dbfs,&EntropyDynamicRecoveryConfigV1::expansion_threshold_dbfs,"dBFS");
 add(egrid,3,"Expansion boost cap","expansionBoost",0,3,initial.max_expansion_boost_db,&EntropyDynamicRecoveryConfigV1::max_expansion_boost_db,"dB");
 auto* output=new QGroupBox("OUTPUT",page);auto* ogrid=new QGridLayout(output);layout->addWidget(output);
 add(ogrid,0,"Total boost cap","totalBoost",0,6,initial.max_total_boost_db,&EntropyDynamicRecoveryConfigV1::max_total_boost_db,"dB");
 add(ogrid,1,"Wet mix","wet",0,1,initial.wet_mix,&EntropyDynamicRecoveryConfigV1::wet_mix,"");
 if(!available)status->setText("MODULE UNAVAILABLE — BYPASS EXACT");
 layout->addStretch();return page;
}
void refreshRecoveryView(QWidget* page,const DspUiSnapshot& snapshot){
 if(!page)return;if(auto* status=page->findChild<QLabel*>("recovery.status")){
  const auto& r=snapshot.diag.recovery;
  status->setText(!r.loaded?"MODULE UNAVAILABLE — BYPASS EXACT":r.active?QString("ACTIVE · FP64 STEREO · REVISION %1").arg(r.revision):QString("BYPASS EXACT · REVISION %1").arg(r.revision));
 }
}
}
