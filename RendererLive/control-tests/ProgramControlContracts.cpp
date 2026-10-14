#include "Engine/Input/ProgramSelection.h"
#include "Engine/Control/ProgramSwitch.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace entropy::input;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct State {
 bool graph=true,gate=true,output=true,resume=false,canceled=false;
 uint32_t demand=1;std::vector<int> events;
};
struct Scheduler {
 State& s;
 void stop(){check(!s.gate,"Graph stopped before output gate closed");s.graph=false;s.events.push_back(2);}
 void start(){check(!s.output,"Graph restarted with old output active");s.graph=true;s.events.push_back(8);}
};
struct Sinks {
 State& s;
 void closePublicationGate(){s.gate=false;s.events.push_back(1);}
 void stop(){check(!s.graph,"Output joined before Graph detachment");s.output=false;s.events.push_back(3);}
};
struct Ingress {
 State& s;
 void require(uint32_t mask){s.demand=mask;s.events.push_back(mask?7:4);}
};
void transaction(bool retainArm,bool disarmDuringPrepare,bool rejectCommit,bool rejectPrepare){
 State s;Scheduler scheduler{s};Sinks sinks{s};Ingress in{s};ProgramSelection selection;
 bool failed=false;
 try {
  const auto seq=entropy::control::switchProgramExclusive(1,scheduler,sinks,in,selection,
   [&]()->uint64_t{
    check(!s.graph&&!s.gate&&!s.output&&s.demand==0,"Control commit not at an isolated boundary");s.events.push_back(5);
    if(rejectCommit)throw std::runtime_error("rejected command");return 42;
   },
   [&]{
    check(!s.graph&&selection.program()==1,"History preparation has wrong ownership/selection");s.events.push_back(6);
    if(disarmDuringPrepare)s.canceled=true;
    if(rejectPrepare)throw std::runtime_error("failed preparation");
   },
   [&]{s.resume=retainArm&&!s.canceled;},
   [&]{s.resume=false;});
  check(seq==42,"Accepted sequence was changed");
 }catch(const std::runtime_error&){failed=true;}
 check(failed==(rejectCommit||rejectPrepare),"Transaction error was hidden/invented");
 check(s.graph&&!s.gate&&!s.output,"The transaction resumed output or left the graph stuck");
 check(selection.program()==(rejectCommit?0u:1u),"Selection and committed command disagree");
 check(s.demand==(rejectCommit?1u:2u),"New Graph requires both input domains");
 check(s.resume==(!failed&&retainArm&&!disarmDuringPrepare),"DISARM/manual ARM intent was lost or recreated");
 if(!failed)check(s.events==std::vector<int>{1,2,3,4,5,6,7,8},"Program switch ownership order changed");
}
}
int main() try {
 ProgramIngress unavailable; // real unstarted input objects; no pipe or DAC opened
 ProgramSelection selection;std::string error;
 check(selection.request(1,unavailable,error),"CINEMA selection was rejected solely because its publisher is absent");
 check(selection.program()==0,"Validation changed scene before command commit");
 selection.quiescent(1);
 check(selection.program()==1&&selection.requiredDomains()==2,"CINEMA selection still waits for MUSIC");
 check(selection.request(0,unavailable,error),"MUSIC selection was rejected solely because its publisher is absent");
 selection.quiescent(0);
 check(selection.program()==0&&selection.requiredDomains()==1,"MUSIC selection still waits for CINEMA");
 check(selection.request(0,unavailable,error),"Same-program request is not idempotent");
 check(!selection.request(2,unavailable,error),"Invalid program accepted");
 check(selection.program()==0&&selection.requiredDomains()==1,"Invalid request changed selection");
 for(unsigned n=0;n<10;++n){const auto p=(n+1)%2;check(selection.request(p,unavailable,error),"Repeated source absence rejected selection");selection.quiescent(p);check(selection.requiredDomains()==(1u<<p),"Rapid alternating selections retained both demands");}
 transaction(true,false,false,false);
 transaction(false,false,false,false);
 transaction(true,true,false,false);
 transaction(true,false,true,false);
 transaction(true,false,false,true);
 std::cout<<"Program control PASS: absent-source admission, both directions, repeated selection, exclusive ownership, rejection, preparation failure and ARM intent. No hardware.\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
