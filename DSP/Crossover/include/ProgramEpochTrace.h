#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

namespace entropy::standalone {
// Metadata-only causal support. Samples and coefficients are never changed.
struct IirEpochTrace {
    uint64_t z1=0,z2=0;
    void reset() noexcept {z1=z2=0;}
    uint64_t process(uint64_t input,double b0,double b1,double b2,double a1,double a2) noexcept {
        const auto output=std::max(b0!=0.0?input:0,z1);
        z1=std::max({b1!=0.0?input:0,a1!=0.0?output:0,z2});
        z2=std::max(b2!=0.0?input:0,a2!=0.0?output:0);
        return output;
    }
};
struct FirEpochTrace {
    std::array<uint64_t,8320> history{};
    size_t write=0;
    void reset() noexcept {history.fill(0);write=0;}
    // Input epochs are monotone. The first nonzero tap therefore carries the
    // newest epoch with a nonzero coefficient, including partitioned-tail taps.
    uint64_t process(uint64_t token,int currentTap,int previousTap,bool previous,double mix) noexcept {
        history[write]=token;
        const auto at=[&](int tap) -> uint64_t {
            return tap>=0 && tap<(int)history.size() ? history[(write+history.size()-tap)%history.size()] : 0;
        };
        const auto result=previous ? std::max(mix>0.0?at(currentTap):0,mix<1.0?at(previousTap):0) : at(currentTap);
        write=(write+1)%history.size();
        return result;
    }
};
}
