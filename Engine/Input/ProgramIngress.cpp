#include "ProgramIngress.h"
#include <windows.h>
#include <algorithm>
namespace entropy::input {
bool ProgramIngress::start(std::wstring music,std::wstring cinema,bool claim) {
 if(owner_)return false;
 if(claim){auto handle=CreateMutexW(nullptr,FALSE,L"Local\\Entropy.Stage3.Render.Owner.V1");
  if(!handle)return false;if(GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(handle);return false;}owner_=handle;}
 if(music.empty())music=defaultEndpoint(Domain::Music);if(cinema.empty())cinema=defaultEndpoint(Domain::Cinema);
 if(!music_.start(Domain::Music,std::move(music))||!cinema_.start(Domain::Cinema,std::move(cinema))){stop();return false;}return true;
}
void ProgramIngress::stop() noexcept {music_.stop();cinema_.stop();if(owner_){CloseHandle(owner_);owner_=nullptr;}}
bool ProgramIngress::configure(std::wstring music,std::wstring cinema){if(!localEndpoint(music)||!localEndpoint(cinema))return false;music_.stop();cinema_.stop();return music_.start(Domain::Music,std::move(music))&&cinema_.start(Domain::Cinema,std::move(cinema));}
void ProgramIngress::restartSubscription(Domain d) noexcept {(d==Domain::Music?music_:cinema_).restart();}
bool ProgramIngress::tryRead(double* const* output,uint32_t domains,GraphInputContext& context) noexcept {
 if(!output||!domains||(domains&~3u))return false;for(unsigned c=0;c<13;++c)if(!output[c])return false;
 require(domains);context={};
 for(unsigned c=0;c<13;++c)std::fill_n(output[c],1024,0.0);
 if(!music_.acquire())return false;if(!cinema_.acquire()){music_.release();return false;}
 bool ok=(!(domains&1)||music_.ready())&&(!(domains&2)||cinema_.ready());
 if(ok){if(domains&1)music_.readLocked(output,context.music);if(domains&2)cinema_.readLocked(output+3,context.cinema);context.domains=domains;}
 cinema_.release();music_.release();return ok;
}
}
