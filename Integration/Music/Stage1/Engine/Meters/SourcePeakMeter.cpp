#include "SourcePeakMeter.h"
#include "../../ThirdParty/NativeSrc/Modules/SRC/R8Brain/src/r8brain-free-src/CDSPResampler.h"
#include <algorithm>
#include <cmath>
#include <vector>
namespace entropy::music {
struct SourcePeakMeter::Impl {
    std::unique_ptr<r8b::CDSPResampler> oversampler[2];
    std::vector<double> scratch[2];
    uint64_t measuredFrames{}, startupFrames{};
    double startup[2]{};
};
SourcePeakMeter::SourcePeakMeter() : impl_(std::make_unique<Impl>()) {}
SourcePeakMeter::~SourcePeakMeter() = default;
void SourcePeakMeter::prepare(uint32_t rate, uint32_t maximum) {
    auto &p = *impl_;
    for (unsigned c = 0; c < 2; c++) {
        p.oversampler[c] = std::make_unique<r8b::CDSPResampler>(
            rate, double(rate) * 4, static_cast<int>(maximum), 2.0, 217.0, r8b::fprLinearPhase);
        p.scratch[c].resize(maximum);
    }
    reset();
}
void SourcePeakMeter::reset() noexcept {
    auto &p = *impl_;
    for (auto &src : p.oversampler)
        if (src)
            src->clear();
    p.measuredFrames = p.startupFrames = 0;
    p.startup[0] = p.startup[1] = 0;
}
PeakReading SourcePeakMeter::process(const double *l, const double *r, uint32_t frames, const double *outL,
                                     const double *outR, uint32_t produced) noexcept {
    auto &p = *impl_;
    PeakReading reading{};
    const double *input[] = {l, r};
    const double *audio[] = {outL, outR};
    double *oversampled[2]{};
    int count[2]{};
    for (unsigned c = 0; c < 2; c++) {
        if (!p.oversampler[c] || frames > p.scratch[c].size())
            return reading;
        for (uint32_t i = 0; i < frames; i++)
            p.scratch[c][i] = std::isfinite(input[c][i]) ? input[c][i] : 0;
        count[c] = p.oversampler[c]->process(p.scratch[c].data(), static_cast<int>(frames), oversampled[c]);
    }
    if (count[0] != count[1] || count[0] < 0)
        return reading;
    double truePeak[2]{};
    for (int i = 0; i < count[0]; i++)
        if (p.measuredFrames + uint64_t(i) >= 8192) {
            reading.truePeakReady = true;
            for (unsigned c = 0; c < 2; c++)
                truePeak[c] = std::max(truePeak[c], std::abs(oversampled[c][i]));
        }
    p.measuredFrames += count[0];
    // Startup window = first 8192 produced samples at 192 kHz (42.667 ms).
    auto available = static_cast<uint32_t>(std::min(uint64_t(produced), 8192 - p.startupFrames));
    for (uint32_t i = 0; i < available; i++)
        for (unsigned c = 0; c < 2; c++)
            p.startup[c] = std::max(p.startup[c], std::abs(audio[c][i]));
    p.startupFrames += available;
    reading.truePeakL = truePeak[0];
    reading.truePeakR = truePeak[1];
    reading.startupPeakL = p.startup[0];
    reading.startupPeakR = p.startup[1];
    reading.startupFramesMeasured = p.startupFrames;
    reading.startupComplete = p.startupFrames == 8192;
    return reading;
}
} // namespace entropy::music
