#include "EgressClockEstimator.h"
#include <algorithm>
#include <cmath>

namespace entropy::egress
{
bool ClockEstimator::prepare(std::uint32_t sinkRate) noexcept
{
    if(sinkRate<8000 || sinkRate>768000){sinkRate_=0;reset();return false;}
    sinkRate_=sinkRate;reset();return true;
}
void ClockEstimator::clearWindow() noexcept
{
    next_=count_=0;bucketCount_=0;bucket_={};
    bucketSinkSum_=bucketHostSum_=bucketNominalSum_=0;
    lastSinkFrame_=lastHost100ns_=lastNominalPublished_=lastFitHost_=0;
    haveLast_=false;cached_={};publishQuality({},{});
}
void ClockEstimator::reset() noexcept
{
#if ENTROPY_EGRESS_FORENSIC_TRACE
    const auto calls=forensic_.observe_calls, resets=forensic_.reset_count;
    forensic_={};forensic_.observe_calls=calls;forensic_.reset_count=resets+1;
    forensic_.reset_site=1;
#endif
    clearWindow();
}
void ClockEstimator::publishQuality(const Fit& shortFit,const Fit& longFit) noexcept
{
    qualitySequence_.fetch_add(1,std::memory_order_acq_rel);
    shortHz_.store(shortFit.valid?shortFit.sinkHz:0,std::memory_order_relaxed);
    longHz_.store(longFit.valid?longFit.sinkHz:0,std::memory_order_relaxed);
    filteredHz_.store(cached_.sinkFramesPerSecond,std::memory_order_relaxed);
    span_.store(longFit.span,std::memory_order_relaxed);
    residual_.store(longFit.residualRms,std::memory_order_relaxed);
    fits_.store(estimates_,std::memory_order_relaxed);
    observationFrame_.store(lastSinkFrame_,std::memory_order_relaxed);
    observationHost_.store(lastHost100ns_,std::memory_order_relaxed);
    estimateValid_.store(cached_.valid,std::memory_order_relaxed);
    qualitySequence_.fetch_add(1,std::memory_order_release);
}
ClockEstimator::Quality ClockEstimator::quality() const noexcept
{
    Quality q;
    q.accepted=accepted_.load(std::memory_order_relaxed);
    q.rejected=rejected_.load(std::memory_order_relaxed);
    for(unsigned attempt=0;attempt<4;++attempt){
        const auto before=qualitySequence_.load(std::memory_order_acquire);
        if(before&1u)continue;
        q.shortWindowHz=shortHz_.load(std::memory_order_relaxed);
        q.longWindowHz=longHz_.load(std::memory_order_relaxed);
        q.filteredHz=filteredHz_.load(std::memory_order_relaxed);
        q.windowSeconds=span_.load(std::memory_order_relaxed);
        q.bucketResidualRmsFrames=residual_.load(std::memory_order_relaxed);
        q.estimates=fits_.load(std::memory_order_relaxed);
        q.valid=estimateValid_.load(std::memory_order_relaxed);
        q.observationFrame=observationFrame_.load(std::memory_order_relaxed);
        q.observationHost100ns=observationHost_.load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        if(before==qualitySequence_.load(std::memory_order_acquire)){q.coherent=true;break;}
    }
    return q;
}
ClockEstimator::Fit ClockEstimator::fit(double windowSeconds) const noexcept
{
    Fit result;if(count_<2)return result;
    const auto oldest=(next_+kCapacity-count_)%kCapacity;
    const auto& newest=samples_[(next_+kCapacity-1)%kCapacity];
    const auto elapsed=[](const Sample& a,const Sample& b){
        return (static_cast<long double>(a.host100ns-b.host100ns)+a.hostOffset-b.hostOffset)*1.0e-7L;
    };
    std::size_t firstOffset=0;
    while(firstOffset+1<count_ && elapsed(newest,samples_[(oldest+firstOffset)%kCapacity])>windowSeconds)++firstOffset;
    const auto& first=samples_[(oldest+firstOffset)%kCapacity];
    result.span=static_cast<double>(elapsed(newest,first));
    const auto n=count_-firstOffset;
    // The long window grows from the original multi-second acquisition range;
    // it does NOT make ARM wait 32 seconds and never delays PCM.
    if(n<32 || result.span<kAcquireSeconds)return result;
    long double sx=0,sxx=0,sy=0,sn=0,sxy=0,sxn=0;
    for(std::size_t k=firstOffset;k<count_;++k){
        const auto& s=samples_[(oldest+k)%kCapacity];
        const auto x=elapsed(s,first);
        const long double y=static_cast<long double>(s.sinkFrame-first.sinkFrame)+s.sinkOffset-first.sinkOffset;
        const long double p=static_cast<long double>(s.nominalPublished-first.nominalPublished)+s.nominalOffset-first.nominalOffset;
        sx+=x;sxx+=x*x;sy+=y;sn+=p;sxy+=x*y;sxn+=x*p;
    }
    const auto den=static_cast<long double>(n)*sxx-sx*sx;
    if(!(den>0))return result;
    const auto slope=(static_cast<long double>(n)*sxy-sx*sy)/den;
    const auto offset=(sy-slope*sx)/n;
    const auto nominalSlope=(static_cast<long double>(n)*sxn-sx*sn)/den;
    result.sinkHz=static_cast<double>(slope);result.nominalHz=static_cast<double>(nominalSlope);
    if(!std::isfinite(result.sinkHz)||result.sinkHz<sinkRate_*.5||result.sinkHz>sinkRate_*1.5)return result;
    long double error=0;
    for(std::size_t k=firstOffset;k<count_;++k){
        const auto& s=samples_[(oldest+k)%kCapacity];
        const auto x=elapsed(s,first);
        const long double y=static_cast<long double>(s.sinkFrame-first.sinkFrame)+s.sinkOffset-first.sinkOffset;
        const auto d=y-(offset+slope*x);error+=d*d;
    }
    result.residualRms=std::sqrt(static_cast<double>(error/n));result.valid=true;return result;
}
ClockEstimate ClockEstimator::cachedResult() noexcept
{
#if ENTROPY_EGRESS_FORENSIC_TRACE
    forensic_.window_seconds=span_.load(std::memory_order_relaxed);
    forensic_.estimate_valid=cached_.valid;
    forensic_.estimate_reason=cached_.valid?ENTROPY_EST_VALID:ENTROPY_EST_WINDOW_SHORT;
    if(cached_.valid){
        forensic_.last_valid_host100ns=lastHost100ns_;
        forensic_.estimated_sink_hz=cached_.sinkFramesPerSecond;
        forensic_.estimated_nominal_hz=cached_.effectiveNominalFramesPerSecond;
        forensic_.estimate_confidence=cached_.confidence;
    }
#endif
    return cached_;
}
ClockEstimate ClockEstimator::observe(const ClockObservation& o) noexcept
{
#if ENTROPY_EGRESS_FORENSIC_TRACE
    ++forensic_.observe_calls;forensic_.estimate_valid=0;
    forensic_.estimate_reason=ENTROPY_EST_NOT_CALLED;
#endif
    if(!sinkRate_){
#if ENTROPY_EGRESS_FORENSIC_TRACE
        rejectForensic(ENTROPY_EST_NOT_PREPARED);
#endif
        rejected_.fetch_add(1,std::memory_order_relaxed);return {};
    }
    if(!o.continuous){
        reset();
#if ENTROPY_EGRESS_FORENSIC_TRACE
        forensic_.reset_site=2;rejectForensic(ENTROPY_EST_DISCONTINUOUS);
#endif
        rejected_.fetch_add(1,std::memory_order_relaxed);return {};
    }
    if(!o.host100ns||!o.sinkFrame||!o.nominalPublished){
#if ENTROPY_EGRESS_FORENSIC_TRACE
        rejectForensic(ENTROPY_EST_ZERO_OBSERVATION);
#endif
        rejected_.fetch_add(1,std::memory_order_relaxed);return {};
    }
    if(haveLast_ && (o.host100ns<=lastHost100ns_ || o.sinkFrame<=lastSinkFrame_ || o.nominalPublished<lastNominalPublished_)){
#if ENTROPY_EGRESS_FORENSIC_TRACE
        rejectForensic(ENTROPY_EST_NONMONOTONIC);
#endif
        rejected_.fetch_add(1,std::memory_order_relaxed);return {};
    }
    if(haveLast_ && o.host100ns-lastHost100ns_>5000000){
        clearWindow();
#if ENTROPY_EGRESS_FORENSIC_TRACE
        ++forensic_.reset_count;forensic_.reset_site=3;
#endif
    }
    lastSinkFrame_=o.sinkFrame;lastHost100ns_=o.host100ns;lastNominalPublished_=o.nominalPublished;haveLast_=true;
    accepted_.fetch_add(1,std::memory_order_relaxed);
#if ENTROPY_EGRESS_FORENSIC_TRACE
    ++forensic_.accepted_observations;forensic_.consecutive_rejections=0;
    forensic_.last_accepted_host100ns=o.host100ns;
#endif
    if(!bucketCount_){bucket_={o.sinkFrame,o.host100ns,o.nominalPublished,0,0,0};bucketSinkSum_=bucketHostSum_=bucketNominalSum_=0;}
    bucketSinkSum_+=static_cast<long double>(o.sinkFrame-bucket_.sinkFrame);
    bucketHostSum_+=static_cast<long double>(o.host100ns-bucket_.host100ns);
    bucketNominalSum_+=static_cast<long double>(o.nominalPublished-bucket_.nominalPublished);++bucketCount_;
    if(o.host100ns-bucket_.host100ns<kBucketTicks)return cachedResult();
    bucket_.sinkOffset=static_cast<double>(bucketSinkSum_/bucketCount_);
    bucket_.hostOffset=static_cast<double>(bucketHostSum_/bucketCount_);
    bucket_.nominalOffset=static_cast<double>(bucketNominalSum_/bucketCount_);
    samples_[next_]=bucket_;next_=(next_+1)%kCapacity;if(count_<kCapacity)++count_;bucketCount_=0;
    const auto longFit=fit(kWindowSeconds),shortFit=fit(kShortSeconds);
    if(!longFit.valid){cached_={};lastFitHost_=0;publishQuality(shortFit,longFit);return cachedResult();}
    const double alpha=cached_.valid&&lastFitHost_
        ? -std::expm1(-static_cast<double>(o.host100ns-lastFitHost_)*1e-7/kFilterSeconds) : 1.0;
    // Low-bandwidth oscillator estimate, not a PCM low-pass. The original
    // per-sample ASRC phase integration and ratio slew remain downstream.
    cached_.sinkFramesPerSecond += alpha*(longFit.sinkHz-cached_.sinkFramesPerSecond);
    cached_.effectiveNominalFramesPerSecond=std::isfinite(longFit.nominalHz)&&longFit.nominalHz>=0?longFit.nominalHz:0;
    cached_.confidence=std::clamp(longFit.span/kWindowSeconds,0.0,1.0);cached_.valid=true;
    lastFitHost_=o.host100ns;++estimates_;publishQuality(shortFit,longFit);return cachedResult();
}
} // namespace entropy::egress
