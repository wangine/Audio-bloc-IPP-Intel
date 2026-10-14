#include "Engine/Input/ProgramSelection.h"
#include <iostream>
int main(){entropy::input::ProgramIngress in;entropy::input::ProgramSelection s;std::string error;
 if(!s.request(1,in,error)||s.program()!=0||s.requiredDomains()!=1||!error.empty()){std::cerr<<"FAIL absent Cinema must be selectable without mutating before commit\n";return 1;}
 // Legacy two-source crossfade state is still available to callers, but the
 // live V1.1 lifecycle uses an exclusive, joined scene boundary instead.
 s.accepted(1);if(s.requiredDomains()!=3)return 2;s.afterBlock(false);if(s.requiredDomains()!=3)return 3;s.afterBlock(true);if(s.requiredDomains()!=2)return 4;
 s.quiescent(1);if(s.requiredDomains()!=2)return 5;
 s.quiescent(0);s.afterBlock(false);if(s.requiredDomains()!=1)return 6;
 if(!s.request(0,in,error)||s.request(2,in,error)||s.program()!=0)return 7;
 std::cout<<"PASS source-independent selection validation, explicit transition and saved program waiting\n";
}
