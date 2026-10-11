#pragma once
#include "InputContract.h"
#include "GraphInputContext.h"
#include "Stage2Ingress.h"
namespace entropy::input {
struct DomainTelemetry : Telemetry { StageBusSessionHeader session{}; StageBusBlockHeader block{}; uint64_t discarded=0,maximumReceiveGapQpc=0,maximumSourceAgeQpc=0; std::array<double,8> peaks{},maximumPeaks{}; };
class ProgramIngress;
class DomainIngress {
public:
 DomainIngress(); ~DomainIngress();
 bool start(Domain,std::wstring); void stop() noexcept; void restart() noexcept;
 void required(bool) noexcept;
 DomainTelemetry telemetry() const noexcept;
 State state() const noexcept;
 bool tryRead(double* const*,StageBusBlockHeader&) noexcept;
private:
 friend class ProgramIngress;
 struct Impl; std::unique_ptr<Impl> impl_;
 bool acquire() noexcept; void release() noexcept; bool ready() const noexcept;
 void readLocked(double* const*,DomainContext&) noexcept;
};
}
