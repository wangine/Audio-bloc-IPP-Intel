#pragma once
#include <array>
#include <cstdint>
#include <algorithm>

namespace entropy::standalone {
// Coefficient construction is CONTROL-thread only. Integer delay remains in
// the existing ring: K+f = [K-(N-1)] + [(N-1)+f]. No extra latency is added.
struct FractionalDelayDesign {
    int mode = 0; // 0 Linear (legacy), 1 Thiran1, 2 Thiran3, 3 Thiran5
    int order = 0;
    int integerTap = 0;
    double delay = 0.0;
    std::array<double,6> a{1.0,0.0,0.0,0.0,0.0,0.0};
    static FractionalDelayDesign compile(int mode, double samples) noexcept;
    bool operator==(const FractionalDelayDesign&) const = default;
};

struct DelaySample { double value = 0.0; uint64_t epoch = 0; };

// Two fixed-size banks. Delay changes crossfade the entire old/new delay path
// over 40 ms rather than mutate poles or retap a running IIR. Rapid edits are
// coalesced at block boundaries after the current fade completes.
class FractionalDelay final {
public:
    void reset() noexcept { banks_={}; active_=0; destination_=1; position_=0; initialized_=false; fading_=false; }
    void request(const FractionalDelayDesign& design, int fadeFrames) noexcept {
        if (!initialized_) { banks_[active_].design=design; initialized_=true; return; }
        if (fading_ || banks_[active_].design==design) return;
        if (!banks_[active_].design.mode && !design.mode) { banks_[active_].design=design; return; }
        destination_=1-active_; banks_[destination_]={}; banks_[destination_].design=design;
        position_=0; frames_=std::max(1,fadeFrames); fading_=true;
    }
    int effectiveOrder() const noexcept { return banks_[fading_?destination_:active_].design.order; }
    bool transitioning() const noexcept { return fading_; }
    template<class Reader> DelaySample process(Reader&& read, DelaySample linear) noexcept {
        auto old=processBank(banks_[active_],read,linear);
        if (!fading_) return old;
        auto next=processBank(banks_[destination_],read,linear);
        double mix=double(++position_)/frames_;
        // A fully destination sample must carry only the destination's epoch
        // (and value), even when an older short-delay path saw newer input.
        if (position_>=frames_) { active_=destination_; fading_=false; return next; }
        DelaySample result{old.value + mix*(next.value-old.value),std::max(old.epoch,next.epoch)};
        return result;
    }
private:
    struct Bank {
        FractionalDelayDesign design{};
        std::array<double,5> z{};
        uint64_t epoch=0;
    };
    template<class Reader> static DelaySample processBank(Bank& bank, Reader& read, DelaySample linear) noexcept {
        const auto& d=bank.design;
        if (!d.mode) return linear;
        auto x=read(d.integerTap);
        if (!d.order) return x; // exact zero/integer bypass; no marginal pole
        const double y=d.a[d.order]*x.value+bank.z[0];
        for(int i=0;i<d.order-1;++i)
            bank.z[i]=d.a[d.order-1-i]*x.value-d.a[i+1]*y+bank.z[i+1];
        bank.z[d.order-1]=x.value-d.a[d.order]*y;
        bank.epoch=std::max(bank.epoch,x.epoch);
        return {y,bank.epoch};
    }
    std::array<Bank,2> banks_{};
    int active_=0,destination_=1,position_=0,frames_=1;
    bool initialized_=false,fading_=false;
};
}
