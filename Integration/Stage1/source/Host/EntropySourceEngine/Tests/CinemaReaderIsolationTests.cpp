
#include "SourceDomainRuntime.h"
#include <windows.h>
#include <thread>
#include <iostream>
static std::atomic<bool> held{false},released{false},done{false};
void cinemaReadBarrier(){held=true;while(!released)Sleep(1);}
namespace entropy::host {struct SourceDomainRaceProbe {static void activate(SourceDomainRuntime& r){r.cinemaPrepared_=true;}};}
using namespace entropy::host;
int main(){SourceDomainRuntime r;SourceDomainRaceProbe::activate(r);SourceBlock1024 b;std::thread reader([&]{r.popCinema1024(b);});auto end=GetTickCount64()+2000;while(!held&&GetTickCount64()<end)Sleep(1);if(!held){released=true;reader.join();return 1;}std::thread writer([&]{r.restartCinema(0);done=true;});Sleep(50);bool waited=!done;bool unavailable=!r.cinemaPrepared();released=true;reader.join();writer.join();if(!waited||!unavailable){std::cerr<<"FAIL CINEMA restart raced a live reader\n";return 2;}if(r.popCinema1024(b)||r.cinemaReady()||r.musicPrepared())return 3;std::cout<<"PASS CINEMA restart excludes new readers and waits for existing reader only\n";}
