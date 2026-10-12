#include "Client/EngineFacade.h"
#include "Client/StartupProfilePrompt.h"
#include "Legacy/RenderWindow.h"
#include <QApplication>
#include <QTimer>
#include <QFontDatabase>
#include <iostream>
int main(int argc,char** argv){
 QApplication app(argc,argv);QCoreApplication::setOrganizationName("EntropyAudioCore");QCoreApplication::setApplicationName("EntropyUI-IPC");
 QString endpoint="EntropyAudioCore.Control.v1",capture;unsigned smoke=0;
 for(int i=1;i+1<argc;++i){auto option=QString::fromUtf8(argv[i]);if(option=="--endpoint")endpoint=QString::fromUtf8(argv[++i]);else if(option=="--smoke-seconds")smoke=QString::fromUtf8(argv[++i]).toUInt();else if(option=="--capture-ui")capture=QString::fromUtf8(argv[++i]);}
 if(qEnvironmentVariable("QT_QPA_PLATFORM")=="offscreen"){QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeuisb.ttf");}
 entropy::ui::EngineFacade facade(endpoint);entropy::stage::RenderWindow window(facade);window.show();
 window.setWindowTitle("Entropy Stage3 0.1.9-v2-r3 — MUSIC / CINEMA");
 entropy::ui::StartupProfilePrompt profilePrompt(facade,window);
 if(smoke)QTimer::singleShot(smoke*1000,&app,[&]{if(!capture.isEmpty())window.grab().save(capture);bool pass=facade.connected()&&facade.blocks()>0;std::cout<<(pass?"EntropyUI executable IPC smoke PASS\n":"EntropyUI executable IPC smoke FAIL\n");app.exit(pass?0:1);});
 return app.exec();
}
