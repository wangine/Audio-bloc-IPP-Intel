#include "Engine/Input/ProgramSelection.h"
#include <iostream>
int main(){entropy::input::ProgramIngress in;entropy::input::ProgramSelection s;std::string error;
 if(s.request(1,in,error)||s.program()!=0||s.requiredDomains()!=1||error.empty()){std::cerr<<"FAIL absent Cinema refused without changing Music\n";return 1;}
 s.accepted(1);if(s.requiredDomains()!=3)return 2;s.afterBlock(false);if(s.requiredDomains()!=3)return 3;s.afterBlock(true);if(s.requiredDomains()!=2)return 4;
 s.quiescent(1);if(s.requiredDomains()!=2)return 5; // saved Cinema without fallback
 s.quiescent(0);s.afterBlock(false);if(s.requiredDomains()!=1)return 6; // Music parameter transition
 std::cout<<"PASS selection refusal, actual-source transition and saved Cinema waiting\n";
}
