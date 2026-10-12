#pragma once
#ifndef WDL_RESAMPLE_FULL_SINC_PRECISION
#define WDL_RESAMPLE_FULL_SINC_PRECISION
#endif
#include "resample.h"
#include <cstdint>
#include <vector>
#include <type_traits>
namespace entropy::host {
// CINEMA-only: a single interleaved 8-channel resampler owns the common phase.
class CinemaWdlSrc final {
public:
    static constexpr unsigned engineId = 2;
    static constexpr unsigned destinationRate = 192000;
    bool prepare(unsigned maximumInputFrames, unsigned taps=64);
    bool configure(unsigned sourceRate) noexcept;
    bool process(const double* const* input, unsigned frames, double* const* output,
                 unsigned capacity, unsigned& produced) noexcept;
    double latencySeconds() noexcept;
    unsigned outputCapacity() const noexcept { return capacity_; }
    bool bypass() const noexcept { return rate_==destinationRate; }
private:
    WDL_Resampler resampler_;
    std::vector<double> output_;
    unsigned maximumInput_=0, capacity_=0, rate_=0;
};
static_assert(std::is_same_v<WDL_ResampleSample,double>);
static_assert(std::is_same_v<WDL_SincFilterSample,double>);
}
