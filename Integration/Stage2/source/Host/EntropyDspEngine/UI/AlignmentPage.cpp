#include "AlignmentPage.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVariant>
namespace entropy::stage {
QString alignmentStatus(const AlignmentSnapshot& a){
 if(!a.loaded||!a.valid)return "ALIGNMENT UNAVAILABLE";
 if(a.rejected)return "LATENCY CONTRACT INVALID — APPLY AGAIN";
 if(a.mode&&a.current==a.proposed&&a.appliedTarget==a.target)return "ALIGNED";
 if(a.mode)return "CHANGE DETECTED — APPLY REQUIRED";
 return "BYPASS EXACT — APPLY TO ALIGN";
}
QWidget* createAlignmentView(QWidget*parent,DspController*controller){
 auto* page=new QWidget(parent);page->setObjectName("alignment.page");auto* layout=new QVBoxLayout(page);layout->setSpacing(16);
 auto addTable=[&](const char* title,const char* key){auto* group=new QGroupBox(title,page);auto* grid=new QGridLayout(group);const char* names[]={"L","R","CX"};for(unsigned i=0;i<3;i++){grid->addWidget(new QLabel(names[i],group),i,0);auto* value=new QLabel("—",group);value->setObjectName(QString("alignment.%1%2").arg(key).arg(i));value->setStyleSheet("font-size:18px;padding:5px;");grid->addWidget(value,i,1);}grid->setColumnStretch(1,1);layout->addWidget(group);};
 addTable("DETECTED PATH LATENCY","detected");auto* target=new QLabel("TARGET —",page);target->setObjectName("alignment.target");layout->addWidget(target);addTable("PROPOSED COMPENSATION","proposed");addTable("CURRENT APPLIED COMPENSATION","current");
 auto* status=new QLabel("ALIGNMENT UNAVAILABLE",page);status->setWordWrap(true);status->setObjectName("alignment.status");status->setStyleSheet("font-size:17px;font-weight:700;color:#f0bd4c;");layout->addWidget(status);
 auto* controls=new QHBoxLayout;auto* bypass=new QPushButton("BYPASS",page);bypass->setObjectName("alignment.bypass");auto* apply=new QPushButton("APPLY",page);apply->setObjectName("alignment.apply");controls->addWidget(bypass);controls->addStretch();controls->addWidget(apply);layout->addLayout(controls);apply->setEnabled(false);bypass->setEnabled(false);
 auto* note=new QLabel("APPLY installs fixed compensation at a block boundary.\nBYPASS removes alignment delay. Center processing stays active.",page);note->setWordWrap(true);layout->addWidget(note);auto* boundary=new QLabel(page);boundary->setObjectName("alignment.boundary");layout->addWidget(boundary);layout->addStretch();
 auto send=[page,controller,status,apply,bypass](bool align){if(!controller)return;auto serial=controller->snapshot().diag.alignment.commandSerial;if(controller->requestAlignment(align)){page->setProperty("alignment.pendingSerial",QVariant::fromValue<qulonglong>(serial+1));apply->setEnabled(false);bypass->setEnabled(false);status->setText("APPLYING AT BLOCK BOUNDARY");}else status->setText("WAIT FOR CURRENT LATENCY — RETRY APPLY");};
 QObject::connect(apply,&QPushButton::clicked,page,[send]{send(true);});QObject::connect(bypass,&QPushButton::clicked,page,[send]{send(false);});return page;
}
void refreshAlignmentView(QWidget* page,const DspUiSnapshot& snapshot){
 if(!page)return;const auto&a=snapshot.diag.alignment;auto value=[](uint64_t samples){return QString("%1 samples   /   %2 ms").arg(samples).arg(samples/192.0,0,'f',3);};
 for(unsigned i=0;i<3;i++){if(auto*l=page->findChild<QLabel*>(QString("alignment.detected%1").arg(i)))l->setText(a.valid?value(a.detected[i]):"—");if(auto*l=page->findChild<QLabel*>(QString("alignment.proposed%1").arg(i)))l->setText(a.valid?value(a.proposed[i]):"—");if(auto*l=page->findChild<QLabel*>(QString("alignment.current%1").arg(i)))l->setText(value(a.current[i]));}
 if(auto*l=page->findChild<QLabel*>("alignment.target"))l->setText(a.valid?"TARGET   "+value(a.target):"TARGET   —");
 const bool pending=page->property("alignment.pendingSerial").toULongLong()>a.commandSerial;
 if(auto*l=page->findChild<QLabel*>("alignment.status"))l->setText(pending?"APPLYING AT BLOCK BOUNDARY":alignmentStatus(a));
 if(auto*p=page->findChild<QPushButton*>("alignment.apply"))p->setEnabled(a.loaded&&a.valid&&!pending);if(auto*p=page->findChild<QPushButton*>("alignment.bypass"))p->setEnabled(a.loaded&&!pending);
 if(auto*l=page->findChild<QLabel*>("alignment.boundary"))l->setText(QString("DSP generation %1   •   Boundary frame %2").arg(a.generation).arg(a.boundaryFrame));
}
}
