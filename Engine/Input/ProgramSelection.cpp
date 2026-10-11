#include "ProgramSelection.h"
namespace entropy::input {
bool ProgramSelection::request(uint32_t p,const ProgramIngress& in,std::string& error) {
 if(p>1){error="Unknown input program";return false;}
 if(p==program())return true;
 if(in.telemetry(p?Domain::Cinema:Domain::Music).state!=State::Receiving){error=p?"WAITING CINEMA: decoder input unavailable; selection unchanged":"WAITING MUSIC: DSP input unavailable; selection unchanged";return false;}
 return true;
}
}
