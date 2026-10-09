#pragma once
namespace entropy::host {
// Producer owns graph execution and invokes project snapshot/commit between
// blocks. DISARM removes audible output; worker/queue/lock checks protect
// mutable control state. Source availability is not a concurrency condition.
inline bool projectControlOperationAllowed(bool armed,bool workerIdle,
    bool queueEmpty,bool heldCommand,bool ownsControlLock,bool adoptionReady=true) noexcept {
    return !armed && workerIdle && queueEmpty && !heldCommand && ownsControlLock && adoptionReady;
}
// Optional absent module bytes are retained, so absence cannot prevent Save.
// A present module's metadata failure still prevents a coherent snapshot.
inline bool projectSaveMetadataReady(bool sourcesReady,bool headroomPresent,bool headroomReady) noexcept {
    return sourcesReady && (!headroomPresent || headroomReady);
}
// A disarmed import still needs an owner block to adopt pending snapshots,
// even when the newly selected program has no available source.
inline bool projectRequiresOwnerCadence(bool armed,bool hasProgramSource,bool pendingImport) noexcept {
    return armed || hasProgramSource || pendingImport;
}
inline bool projectRequiresMusicAdoption(bool musicRequired,bool armed,bool pendingImport) noexcept {
    return musicRequired || (!armed && pendingImport);
}
// Save drains accepted optional MUSIC controls even when CINEMA is selected
// and output is armed. This services the inactive chain without changing the
// selected Crossover program; Load keeps its existing disarmed adoption rule.
inline bool projectSaveRequiresMusicAdoption(bool musicRequired,bool armed,bool pendingImport,bool pendingSave) noexcept {
    return pendingSave || projectRequiresMusicAdoption(musicRequired,armed,pendingImport);
}
inline bool projectArmAllowed(bool projectIoPending,bool projectAdoptionPending) noexcept {
    return !projectIoPending && !projectAdoptionPending;
}
}
