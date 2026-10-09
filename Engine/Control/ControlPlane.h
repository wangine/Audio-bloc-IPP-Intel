#pragma once
#include "CrossoverAccess.h"
#include "Engine/ParameterSnapshot.h"
namespace entropy::control {
class ControlPlane {
public:
 explicit ControlPlane(engine::AudioGraph&,uint64_t session=1);
 ipc::Value metadata() const;
 ipc::Value snapshot() const;
 uint64_t execute(const ipc::Value&);
 void atBlockBoundary() noexcept;
 void drainAtQuiescentBoundary(); // Control owner only AFTER scheduler.stop()/join.
 void afterBlock() noexcept;
 CrossoverAccess& crossover(){return crossover_;}
 ParameterSnapshot appliedState() const;
 ParameterSnapshot desired() const{return desired_;} // control thread only
 void replace(const ParameterSnapshot&); // validated load, control owner
 void setRenderLaneMask(uint32_t); // control owner; configured lanes, never ARM state
 bool settled() const noexcept{return queue_.empty()&&applied_.load(std::memory_order_acquire)>=lastQueued_&&crossover_.settled();}
private:
 void synchronizeMatrixSources() noexcept;
 void synchronizeHeadroomLedger() noexcept;
 engine::AudioGraph& graph_;CrossoverAccess crossover_;SnapshotQueue queue_;
 ParameterSnapshot desired_;uint64_t session_,sequence_=0,lastQueued_=0;
 std::atomic<uint64_t> accepted_{0},applied_{0},matrixApplied_{0},headroomApplied_{0};std::atomic<unsigned> applyFault_{0};
 std::atomic<uint32_t> renderLaneMask_{0xffffu};
};
}
