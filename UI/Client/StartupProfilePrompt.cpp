#include "StartupProfilePrompt.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QAction>
namespace entropy::ui {
StartupProfilePrompt::StartupProfilePrompt(EngineFacade& facade,QWidget& window):facade_(facade),window_(window){
    timer_.setInterval(200);connect(&timer_,&QTimer::timeout,this,[this]{offer();});timer_.start();
    auto* retry=new QAction("Create or load AudioCore profile",&window_);retry->setShortcut(QKeySequence("Ctrl+Shift+P"));window_.addAction(retry);connect(retry,&QAction::triggered,this,[this]{requestSelection();});
}
void StartupProfilePrompt::offer(){
    if(prompted_||!facade_.connected()||!facade_.profileRequired())return;prompted_=true;
    QMessageBox choice(QMessageBox::Information,"AudioCore profile","AudioCore is waiting for a validated profile.\nCreate a new profile or load an existing one. DACs remain DISARMED.\n"+facade_.profileError(),QMessageBox::NoButton,&window_);
    auto* create=choice.addButton("Create new profile",QMessageBox::AcceptRole);auto* load=choice.addButton("Load existing profile",QMessageBox::ActionRole);choice.addButton("Keep waiting",QMessageBox::RejectRole);choice.exec();
    bool make=choice.clickedButton()==create;if(!make&&choice.clickedButton()!=load)return;
    auto path=make?QFileDialog::getSaveFileName(&window_,"Create AudioCore profile",facade_.profilePath(),"Entropy profile (*.entropy)"):QFileDialog::getOpenFileName(&window_,"Load AudioCore profile",facade_.profilePath(),"Entropy profile (*.entropy)");
    if(path.isEmpty())return;
    facade_.request({{"action",make?"new-profile":"load-profile"},{"path",path}},[this](const QJsonObject& reply){if(!reply["ok"].toBool()){QMessageBox::warning(&window_,"Profile refused",reply["error"].toString());prompted_=false;}});
}
}
