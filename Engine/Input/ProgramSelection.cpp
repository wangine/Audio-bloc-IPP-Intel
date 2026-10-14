#include "ProgramSelection.h"
namespace entropy::input {
bool ProgramSelection::request(uint32_t p,const ProgramIngress&,std::string& error) {
 error.clear();
 if(p>1){error="Unknown input program";return false;}
 // This is command validation, not source admission. A user can select an
 // absent MUSIC/CINEMA publisher. The lifecycle holds output until actual
 // validated PCM arrives; lack of audio must not reject or revert the choice.
 return true;
}
}
