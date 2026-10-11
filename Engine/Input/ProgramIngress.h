#pragma once
#include "DomainIngress.h"
namespace entropy::input {
class ProgramIngress {
public:
 ProgramIngress()=default; ~ProgramIngress(){stop();}
 bool start(std::wstring music={},std::wstring cinema={},bool claimRenderOwner=true);
 void stop() noexcept; void restartSubscription(Domain) noexcept;
 bool configure(std::wstring music,std::wstring cinema);
 bool tryRead(double* const* output13,uint32_t requiredDomains,GraphInputContext&) noexcept;
 DomainTelemetry telemetry(Domain d) const noexcept {return d==Domain::Music?music_.telemetry():cinema_.telemetry();}
 void require(uint32_t domains) noexcept {music_.required(domains&1);cinema_.required(domains&2);}
 bool faulted(uint32_t domains) const noexcept {return ((domains&1)&&music_.state()==State::Fault)||((domains&2)&&cinema_.state()==State::Fault);}
private:
 DomainIngress music_,cinema_; void* owner_=nullptr;
};
}
