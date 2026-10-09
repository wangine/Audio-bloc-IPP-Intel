#include "SourceWorkerWake.h"
#include "SourceReadiness.h"
#include <thread>
#include <chrono>
#include <iostream>
#include <cstdlib>
using namespace entropy::host;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<#x<<"\n";std::exit(1);}}while(0)
int main(){SourceWorkerWake w;CHECK(w.valid());auto r=w.snapshot();w.notify();auto before=std::chrono::steady_clock::now();w.waitForChange(r);CHECK(std::chrono::steady_clock::now()-before<std::chrono::milliseconds(100));
for(unsigned i=0;i<200;++i){std::atomic<bool> entered{false},finished{false},stop{false};r=w.snapshot();std::thread waiter([&]{entered=true;w.waitForChange(r);finished=true;});while(!entered)std::this_thread::yield();if(i%3==0)Sleep(1);w.notify();w.notify();waiter.join();CHECK(finished);CHECK(w.snapshot()!=r);}
// An old coalesced event can cause a spurious wake. The work loop must recheck.
r=w.snapshot();w.notify();w.notify();w.waitForChange(r);std::atomic<bool> stopped{false};r=w.snapshot();std::thread waiter([&]{while(!stopped){auto observed=w.snapshot();if(!stopped)w.waitForChange(observed);}});Sleep(5);stopped=true;w.notify();waiter.join();
SourceReadiness ready;ready=false;ready.produced(1024,8192);CHECK(!ready.load());ready.produced(8192,8192);CHECK(ready.load());auto observation=ready.revision();ready=false;ready.produced(8192,8192);ready.drained(observation,0);CHECK(ready.load());observation=ready.revision();ready.drained(observation,0);CHECK(!ready.load());ready.produced(4096,8192);CHECK(!ready.load());ready.produced(8192,8192);CHECK(ready.load());
std::cout<<"PASS notify before/during wait, coalescing, recheck, stop; ready epoch/preroll/drain/refill\n";}
