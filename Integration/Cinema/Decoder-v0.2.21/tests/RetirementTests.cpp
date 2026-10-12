#include "CinemaOutput.h"
#include "StagePublishV2.h"
#include <windows.h>
#include <iostream>
int main(){CinemaOutput output;std::string error;const auto endpoint=CinemaOutput::defaultEndpoint()+L".Retirement."+std::to_wstring(GetCurrentProcessId());if(!output.start(6,endpoint,error))return 1;
 const auto startupToken=output.generation();output.invalidate();std::array<float,8> stale{};stale.fill(.75f);for(int i=0;i<4096;i++)output.submit(stale,startupToken);Sleep(100);if(output.blocks()!=0){std::cerr<<"FAIL inactive generation did not retire extracted first block\n";return 1;}
 const auto token=output.generation();std::array<float,8> old{};old.fill(.75f);for(int i=0;i<64;i++)output.submit(old,token);output.invalidate();Sleep(20);const auto before=output.blocks();for(int i=0;i<4096;i++)output.submit(old,token);Sleep(100);if(output.blocks()!=before){std::cerr<<"FAIL retired pacer frames reactivated new generation\n";return 1;}
 const auto next=output.generation();if(next<=token)return 1;for(int i=0;i<4096;i++)output.submit({},next);Sleep(100);if(output.blocks()==before)return 1;output.stop();std::cout<<"PASS retired producer token rejected; new source admitted; no stale reactivation\n";return 0;}
