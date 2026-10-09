#include "SourceController.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <iostream>
using namespace entropy::host;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<" "<<#x<<'\n';return 1;}}while(0)
template<class F>bool until(F f){for(unsigned n=0;n<2000;++n){if(f())return true;Sleep(1);}return false;}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir tmp;SourceController c;std::string error;CHECK(c.start(TEST_MODULE_ROOT,std::filesystem::path(tmp.path().toStdWString()),error,true));CHECK(until([&]{return c.blocks()>4;}));CHECK(c.snapshot().state.engines==(std::array<uint32_t,3>{0,0,2}));SourceCommand cmd;cmd.type=SourceCommand::Engine;cmd.role=2;cmd.value=0;CHECK(c.command(cmd));CHECK(until([&]{return c.snapshot().state.engines[2]==0;}));cmd.role=0;CHECK(c.command(cmd));Sleep(20);CHECK(c.snapshot().state.engines==(std::array<uint32_t,3>{0,0,0}));cmd.role=2;cmd.value=2;CHECK(c.command(cmd));CHECK(until([&]{return c.snapshot().state.engines[2]==2;}));cmd.value=1;CHECK(c.command(cmd));Sleep(30);CHECK(c.snapshot().state.engines==(std::array<uint32_t,3>{0,0,2}));CHECK(c.snapshot().faults==0);c.stop();std::cout<<"PASS CINEMA selector0/2, WDLdefault, MUSIC/AUXunchanged, invalid1rejected\n";}
