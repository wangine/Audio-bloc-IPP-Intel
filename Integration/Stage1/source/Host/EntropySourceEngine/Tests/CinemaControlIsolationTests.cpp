
#include "SourceController.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <atomic>
#include <iostream>
static std::atomic<bool> entered{false},releaseBarrier{false};
void engineCommandBarrier(){entered=true;while(!releaseBarrier)Sleep(1);}
namespace entropy::host {struct SourceLinkRaceProbe {
 static void engine(SourceController& c,unsigned role,unsigned value){std::lock_guard lock(c.mutex_);SourceCommand cmd;cmd.type=SourceCommand::Engine;cmd.role=role;cmd.value=value;c.capturing_=true;c.service(cmd);c.capturing_=false;}
};}
using namespace entropy::host;
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir tmp;SourceController c;std::string e;if(!c.start(TEST_MODULE_ROOT,std::filesystem::path(tmp.path().toStdWString()),e,true))return 1;
for(auto value:{2u,0u,1u}){entered=false;releaseBarrier=false;std::thread control([&]{SourceLinkRaceProbe::engine(c,2,value);});auto end=GetTickCount64()+2000;while(!entered&&GetTickCount64()<end)Sleep(1);auto before=c.blocks();Sleep(120);auto after=c.blocks();releaseBarrier=true;control.join();if(!entered||after<=before+5){c.stop();std::cerr<<"FAIL common audio worker stopped during CINEMA engine command value="<<value<<" before="<<before<<" after="<<after<<'\n';return 2;}}
c.stop();std::cout<<"PASS common audio worker advances during blocked CINEMA same/different/invalid engine commands\n";}
