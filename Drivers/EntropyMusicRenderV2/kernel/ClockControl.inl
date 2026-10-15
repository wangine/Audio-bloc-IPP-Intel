// Included once by PcmCommit.cpp. No DSP or PCM ownership changes.
NTSTATUS PcmCommit::ClockControl(const entropy::music::clockwire::Request& request,uint64_t newLease,entropy::music::clockwire::Reply& output){
    using namespace entropy::music;
    if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    if(!TryEnter())return STATUS_DEVICE_NOT_READY;
    FollowResult result=FollowResult::Ok;
    KIRQL old;KeAcquireSpinLock(&lock,&old);
    // Linearization point shared with the Submit clock/QPC pair. Retuning is
    // bounded integer metadata work; no payload copy or blocking under lock.
    const auto now=static_cast<uint64_t>(KeQueryPerformanceCounter(nullptr).QuadPart);
    const auto kind=clockwire::Kind(request.header.kind);
    if(kind==clockwire::Kind::Acquire)result=timeline.acquireClock(now,request.streamGeneration,newLease,request.masterEpoch);
    else if(kind==clockwire::Kind::Update)result=timeline.updateClock(now,request.streamGeneration,request.lease,request.masterEpoch,request.sequence,request.correctionPpb);
    else if(kind==clockwire::Kind::Release)result=timeline.releaseClock(now,request.streamGeneration,request.lease,request.masterEpoch);
    output=clockwire::reply(kind,timeline.clockStatus(now));
    cachedClock_=timeline.clockSnapshot();
    KeReleaseSpinLock(&lock,old);
    Leave();
    switch(result){
        case FollowResult::Ok:return STATUS_SUCCESS;
        case FollowResult::Busy:return STATUS_DEVICE_BUSY;
        case FollowResult::Stale:return STATUS_REVISION_MISMATCH;
        case FollowResult::NotRunning:return STATUS_DEVICE_NOT_READY;
        case FollowResult::Invalid:return STATUS_INVALID_PARAMETER;
        default:return STATUS_INVALID_DEVICE_STATE;
    }
}
void PcmCommit::ReleaseClockHandle(uint64_t lease) noexcept {
    if(!lease || !TryEnter())return; // lease expiry is the bounded fallback
    KIRQL old;KeAcquireSpinLock(&lock,&old);
    const auto now=static_cast<uint64_t>(KeQueryPerformanceCounter(nullptr).QuadPart);
    timeline.releaseClockHandle(now,lease);cachedClock_=timeline.clockSnapshot();
    KeReleaseSpinLock(&lock,old);Leave();
}
