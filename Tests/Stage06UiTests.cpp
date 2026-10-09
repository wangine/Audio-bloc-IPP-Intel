#include "Client/StartupProfilePrompt.h"
#include "Legacy/RenderWindow.h"
#include <QApplication>
#include <QMessageBox>
#include <QAbstractButton>
#include <iostream>
int main(int argc,char** argv){
    QApplication app(argc,argv);entropy::ui::EngineFacade facade(argv[1]);entropy::stage::RenderWindow window(facade);window.show();entropy::ui::StartupProfilePrompt prompt(facade,window);
    bool checked=true;unsigned dialogs=0;
    QTimer poll;poll.setInterval(100);QObject::connect(&poll,&QTimer::timeout,&app,[&]{
        auto* dialog=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());if(!dialog)return;
        bool create=false,load=false;QAbstractButton* wait=nullptr;
        for(auto* button:dialog->buttons()){create|=button->text()=="Create new profile";load|=button->text()=="Load existing profile";if(button->text()=="Keep waiting")wait=button;}
        checked=checked&&create&&load&&wait&&facade.connected()&&facade.profileRequired();if(wait)wait->click();++dialogs;
        if(dialogs==1)QTimer::singleShot(200,&app,[&]{prompt.requestSelection();});
        else {QTimer::singleShot(200,&app,[&]{std::cout<<(checked?"Stage06 actual Qt first-run Create/Load/Keep waiting and reopening dialog PASS\n":"Stage06 Qt first-run dialog FAIL\n");app.exit(checked?0:1);});poll.stop();}
    });poll.start();QTimer::singleShot(6000,&app,[&]{if(dialogs<2)app.exit(1);});return app.exec();
}
