#pragma once
#include "DspStageRuntime.h"
namespace entropy::stage {
// Retired V1 demand fixture, linked only in DspLegacyFixtures tests.
class DspLocalDemand {
public:
 bool attach(const StageBusSessionHeader&,const StageBusProgramSnapshot&,uint64_t revision,std::string&);
 DspStepResult step(uint64_t now) noexcept;
 void beginQuiesce() noexcept {quiescing_=true;}
 bool pending() const noexcept {return pending_;}
 uint64_t blocks() const noexcept {return blocks_;}
 uint64_t scheduleRebases() const noexcept {return scheduleRebases_;}
 const StageBusBlockHeader& lastHeader() const noexcept {return last_;}
 const auto& planes() const noexcept {return planes_;}
 void close() noexcept;
private:
 StageBusWin32 bus_;StageBusSessionHeader session_{};StageBusProgramSnapshot program_{};
 StageBusFrameRequest request_{};StageBusBlockHeader last_{};
 std::array<std::array<double,1024>,11> planes_{};std::array<double*,11> pointers_{};
 uint64_t revision_=0,sequence_=1,due_=0,started_=0,blocks_=0,dueRemainder_=0,scheduleRebases_=0;
 bool attached_=false,pending_=false,quiescing_=false;
};
}
