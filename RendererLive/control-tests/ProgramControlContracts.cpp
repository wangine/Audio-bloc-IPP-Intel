#include "Engine/Input/ProgramSelection.h"
#include <iostream>
#include <stdexcept>
using namespace entropy::input;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
}
int main() try {
 // Real ingress objects, deliberately never started. No publishers, pipes,
 // endpoints, audio streams or playback commands are created by this test.
 ProgramIngress unavailable;
 ProgramSelection selection;
 std::string error;
 check(selection.request(1,unavailable,error),"CINEMA selection was rejected solely because its publisher is absent");
 check(selection.program()==0,"Validation changed the selected scene before command commit");
 selection.quiescent(1);
 check(selection.program()==1&&selection.requiredDomains()==2,"Exclusive CINEMA selection still waits for MUSIC");
 check(selection.request(0,unavailable,error),"MUSIC selection was rejected solely because its publisher is absent");
 selection.quiescent(0);
 check(selection.program()==0&&selection.requiredDomains()==1,"Exclusive MUSIC selection still waits for CINEMA");
 check(selection.request(0,unavailable,error),"Repeated same-program selection is not idempotent");
 check(!selection.request(2,unavailable,error),"An invalid program was accepted");
 check(selection.program()==0&&selection.requiredDomains()==1,"Invalid request changed selection");
 std::cout<<"Program admission PASS: absent sources, both directions, idempotence, invalid request; no hardware.\n";
 return 0;
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
