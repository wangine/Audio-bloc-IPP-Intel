#pragma once
#include <chrono>
#include <cstdint>

namespace entropy::input {
// Calendar policy from the accepted V07 EngineCadencePolicy. This schedules
// requests only: it never changes samples, sequence, timestamps or DSP state.
struct IngressCadence {
    static constexpr uint64_t elapsedNanoseconds(uint64_t blocks) noexcept {
        return (blocks / 192000) * 1024 * 1000000000ULL +
            ((blocks % 192000) * 1024 * 1000000000ULL) / 192000;
    }
    static auto next(std::chrono::steady_clock::time_point scheduled,
                     std::chrono::steady_clock::time_point now,
                     std::chrono::steady_clock::time_point begin,
                     bool outputAtTarget) noexcept {
        constexpr auto cycle=std::chrono::nanoseconds(1024ULL*1000000000/192000);
        if(outputAtTarget && now>scheduled && now-scheduled>cycle)scheduled=now+cycle;
        const auto earliest=begin+cycle*7/8;
        const auto paced=scheduled<earliest?earliest:scheduled;
        return paced<now?now:paced;
    }
};
}
