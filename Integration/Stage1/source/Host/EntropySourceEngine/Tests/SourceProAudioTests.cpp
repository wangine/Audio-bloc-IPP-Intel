#include "ProAudioThreadScope.h"
#include <thread>
#include <iostream>
#include <cstring>
#include <cstdlib>
using namespace entropy::host;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<#x<<"\n";std::exit(1);}}while(0)
static unsigned reverted=0;
static HANDLE WINAPI reg(LPCWSTR,LPDWORD){return reinterpret_cast<HANDLE>(1);}
static HANDLE WINAPI failReg(LPCWSTR,LPDWORD){return nullptr;}
static BOOL WINAPI priority(HANDLE,AVRT_PRIORITY){return TRUE;}
static BOOL WINAPI failPriority(HANDLE,AVRT_PRIORITY){return FALSE;}
static BOOL WINAPI revert(HANDLE){++reverted;return TRUE;}
int main(){ {ProAudioThreadScope x({reg,priority,revert});CHECK(x.active());}CHECK(reverted==1);{ProAudioThreadScope x({failReg,priority,revert});CHECK(!x.active());CHECK(std::strstr(x.failureReason(),"register"));}CHECK(reverted==1);{ProAudioThreadScope x({reg,failPriority,revert});CHECK(!x.active());CHECK(std::strstr(x.failureReason(),"priority"));}CHECK(reverted==2);{ProAudioThreadScope x({nullptr,priority,revert});CHECK(!x.active());CHECK(std::strstr(x.failureReason(),"incomplete"));}
 std::thread worker([]{ProAudioThreadScope actual;CHECK(actual.active());std::cout<<"PASS actual worker registration and AVRT priority succeeded\n";});worker.join();}
