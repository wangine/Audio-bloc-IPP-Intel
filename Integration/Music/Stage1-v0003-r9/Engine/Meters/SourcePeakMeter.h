#pragma once
#include <cstdint>
#include <memory>
namespace entropy::music {
struct PeakReading {
    double truePeakL{}, truePeakR{}, startupPeakL{}, startupPeakR{};
    uint64_t startupFramesMeasured{};
    bool truePeakReady{}, startupComplete{};
};
// Read-only analysis branch. No returned sample is ever used as audio output.
class SourcePeakMeter {
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    SourcePeakMeter();
    ~SourcePeakMeter();
    void prepare(uint32_t rate, uint32_t maximumFrames);
    void reset() noexcept;
    PeakReading process(const double *l, const double *r, uint32_t frames, const double *outL,
                        const double *outR, uint32_t produced) noexcept;
};
} // namespace entropy::music
