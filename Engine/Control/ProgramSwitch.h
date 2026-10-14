#pragma once
#include <cstdint>
#include <stdexcept>
namespace entropy::control {
// Lifecycle/control owner ONLY. A scene change is an explicit stream boundary,
// not a request for two independently arriving sources to advance together.
// No clock/filter/gain math is implemented here and no PCM is manufactured.
// The callbacks keep production ownership ordering testable without opening a
// physical device. commitControl is transactional; prepareControl may reset the
// old scene's histories only after the Graph has been joined and sinks stopped.
template<class Scheduler,class Sinks,class Ingress,class Selection,
         class Commit,class Prepare,class Await,class Cancel>
uint64_t switchProgramExclusive(uint32_t target,Scheduler& scheduler,Sinks& sinks,
 Ingress& ingress,Selection& selection,Commit commitControl,Prepare prepareControl,
 Await awaitRealSource,Cancel cancelResume) {
 if(target>1)throw std::invalid_argument("Unknown input program");
 sinks.closePublicationGate();
 scheduler.stop(); // join, then acknowledge detachment; never reset a live owner
 sinks.stop();
 ingress.require(0); // neither queue is an active playback stream during compile
 bool restartAttempted=false;
 try {
  const uint64_t sequence=commitControl();
  selection.quiescent(target);
  prepareControl();
  ingress.require(selection.requiredDomains()); // exactly the selected domain
  awaitRealSource(); // preserve existing ARM intent, never create new intent
  restartAttempted=true;
  scheduler.start(); // the Graph waits for real PCM; output remains stopped
  return sequence;
 } catch(...) {
  cancelResume();
  ingress.require(selection.requiredDomains());
  if(!restartAttempted)scheduler.start();
  throw;
 }
}
}
