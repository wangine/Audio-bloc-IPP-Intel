#include "Engine/Control/ConsoleShutdown.h"
#include <thread>
#include <iostream>

int main() {
    entropy::control::ConsoleShutdown shutdown;
    std::atomic<bool> returned{false};
    std::thread handler([&]{shutdown.handle(CTRL_CLOSE_EVENT);returned=true;});
    const auto deadline=GetTickCount64()+2000;
    while(!shutdown.requested()&&GetTickCount64()<deadline)Sleep(1);
    Sleep(30);
    const bool requested=shutdown.requested(),returnedBeforeCleanup=returned.load();
    shutdown.complete();handler.join();
    if(!requested||returnedBeforeCleanup){std::cerr<<"FAIL: console close returned before transport cleanup completed\n";return 1;}
    shutdown.reset();
    if(!shutdown.handle(CTRL_BREAK_EVENT)||!shutdown.requested()){std::cerr<<"FAIL: CTRL_BREAK must request shutdown\n";return 1;}
    if(shutdown.handle(CTRL_LOGOFF_EVENT)){std::cerr<<"FAIL: unhandled console event changed behavior\n";return 1;}
    std::cout<<"PASS: console close waits for completed transport/DAC cleanup; break remains nonblocking\n";
}
