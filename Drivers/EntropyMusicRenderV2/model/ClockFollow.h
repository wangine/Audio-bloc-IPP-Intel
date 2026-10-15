#pragma once
#include "Clock.h"
namespace entropy::music {
enum class FollowResult:uint32_t {Ok=0,Invalid=1,Busy=2,Stale=3,NotRunning=4,ClockError=5};
enum class FollowState:uint32_t {NoStream=0,Autonomous=1,Acquired=2,Following=3,Expired=4};
struct FollowSnapshot {
    uint64_t generation=0,lease=0,masterEpoch=0,sequence=0,frequency=0,now=0,expires=0,frames=0;
    int32_t requestedPpb=0,appliedPpb=0;
    FollowState state=FollowState::NoStream;
    uint32_t rate=0;
    uint64_t accepted=0,rejected=0;
};
// Serialized, handle-independent lease model. The kernel maps a unique lease
// token to a file handle; source generation and master epoch are separate.
class ClockFollow {
    uint64_t lease_=0,master_=0,generation_=0,sequence_=0,last_=0,expires_=0,accepted_=0,rejected_=0;
    int32_t requested_=0,applied_=0;
    FollowResult reject(FollowResult r) noexcept {++rejected_;return r;}
    bool deadline(const Clock& clock,uint64_t now,uint64_t& until) const noexcept {
        if(clock.frequency()>UINT64_MAX/2)return false;
        const auto period=clock.frequency()*2;
        if(now>UINT64_MAX-period)return false;
        until=now+period;return true;
    }
public:
    static constexpr uint32_t LeaseMilliseconds=2000,SlewPpbPerSecond=20000;
    void reset() noexcept {*this=ClockFollow{};}
    void invalidate() noexcept {lease_=master_=generation_=sequence_=last_=expires_=0;requested_=applied_=0;}
    FollowResult acquire(Clock& clock,uint64_t now,uint64_t generation,uint64_t lease,uint64_t master) noexcept {
        if(!clock.running())return reject(FollowResult::NotRunning);
        if(!lease || !master)return reject(FollowResult::Invalid);
        if(generation!=clock.generation())return reject(FollowResult::Stale);
        if(lease_ && generation_==generation && now<expires_)return reject(FollowResult::Busy);
        uint64_t until=0;if(!deadline(clock,now,until) || !clock.discipline(now,0,until))return reject(FollowResult::ClockError);
        lease_=lease;master_=master;generation_=generation;sequence_=0;last_=now;expires_=until;requested_=applied_=0;
        return FollowResult::Ok;
    }
    FollowResult update(Clock& clock,uint64_t now,uint64_t generation,uint64_t lease,uint64_t master,uint64_t sequence,int32_t ppb) noexcept {
        if(!clock.running())return reject(FollowResult::NotRunning);
        if(ppb<-Clock::MaxCorrectionPpb || ppb>Clock::MaxCorrectionPpb || !sequence || sequence==UINT64_MAX)return reject(FollowResult::Invalid);
        if(!lease_ || generation!=clock.generation() || generation!=generation_ || lease!=lease_ || master!=master_ ||
           sequence<=sequence_ || now<last_ || now>=expires_)return reject(FollowResult::Stale);
        uint64_t limit=0,until=0;
        if(!multiplyDivide(now-last_,SlewPpbPerSecond,clock.frequency(),limit) || !deadline(clock,now,until))return reject(FollowResult::ClockError);
        const int32_t difference=ppb-applied_;
        const auto magnitude=static_cast<uint32_t>(difference<0?-difference:difference);
        const auto step=static_cast<int32_t>(limit<magnitude?limit:magnitude);
        const int32_t next=applied_+(difference<0?-step:step);
        if(!clock.discipline(now,next,until))return reject(FollowResult::ClockError);
        requested_=ppb;applied_=next;last_=now;expires_=until;sequence_=sequence;++accepted_;return FollowResult::Ok;
    }
    FollowResult release(Clock& clock,uint64_t now,uint64_t generation,uint64_t lease,uint64_t master) noexcept {
        if(!lease_ || lease!=lease_ || generation!=generation_ || generation!=clock.generation() || master!=master_)return reject(FollowResult::Stale);
        if(clock.running() && !clock.discipline(now,0,0))return reject(FollowResult::ClockError);
        invalidate();return FollowResult::Ok;
    }
    bool releaseHandle(Clock& clock,uint64_t now,uint64_t lease) noexcept {
        if(!lease || lease!=lease_)return false;
        return release(clock,now,generation_,lease,master_)==FollowResult::Ok;
    }
    FollowSnapshot snapshot(const Clock& clock,uint64_t now) const noexcept {
        FollowSnapshot s{};s.generation=clock.generation();s.lease=lease_;s.masterEpoch=master_;s.sequence=sequence_;
        s.frequency=clock.frequency();s.now=now;s.expires=expires_;s.requestedPpb=requested_;s.appliedPpb=clock.correctionPpb(now);
        s.rate=clock.nominalRate();s.accepted=accepted_;s.rejected=rejected_;
        s.state=!clock.running()?FollowState::NoStream:!lease_?FollowState::Autonomous:
            now>=expires_?FollowState::Expired:sequence_?FollowState::Following:FollowState::Acquired;
        if(!clock.position(now,s.frames))s.state=FollowState::NoStream;
        return s;
    }
};
}
