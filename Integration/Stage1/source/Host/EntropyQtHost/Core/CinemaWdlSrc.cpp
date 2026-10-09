#include "CinemaWdlSrc.h"
#include <algorithm>
#include <cstring>
namespace entropy::host {
bool CinemaWdlSrc::prepare(unsigned maximumInputFrames, unsigned taps) {
    if (!maximumInputFrames || maximumInputFrames>1024 || (taps!=64 && taps!=128)) return false;
    maximumInput_=maximumInputFrames;
    capacity_=maximumInputFrames*24+128; // smallest supported input rate: 8000 Hz.
    try { output_.resize(static_cast<size_t>(capacity_)*8); }
    catch (...) { return false; }
    resampler_.SetMode(false,0,true,static_cast<int>(taps),32);
    resampler_.SetFeedMode(true);
    // Reserve the largest ideal coefficient table (64 phases) without feeding
    // any audio. All later rates reuse it, including noninteger SRC ratios.
    resampler_.SetRates(3000,destinationRate);
    resampler_.Prealloc(8,static_cast<int>(maximumInput_+128),0);
    resampler_.Reset();
    rate_=0;
    return true;
}
bool CinemaWdlSrc::configure(unsigned sourceRate) noexcept {
    if (!maximumInput_ || sourceRate<8000 || sourceRate>768000) return false;
    rate_=sourceRate;
    if (bypass()) return true; // no WDL calls, including ratio-1 configuration.
    resampler_.Reset();
    resampler_.SetRates(sourceRate,destinationRate);
    resampler_.Prealloc(8,static_cast<int>(maximumInput_+128),0);
    return true;
}
bool CinemaWdlSrc::process(const double* const* input, unsigned frames,
                          double* const* output, unsigned capacity,
                          unsigned& produced) noexcept {
    produced=0;
    if (!rate_ || !input || !output || !frames || frames>maximumInput_ || capacity<capacity_) return false;
    for (unsigned c=0;c<8;++c) if (!input[c] || !output[c]) return false;
    if (bypass()) {
        for (unsigned c=0;c<8;++c) std::memcpy(output[c],input[c],frames*sizeof(double));
        produced=frames;
        return true;
    }
    double* interleaved=nullptr;
    if (resampler_.ResamplePrepare(static_cast<int>(frames),8,&interleaved)!=static_cast<int>(frames) || !interleaved) return false;
    for (unsigned i=0;i<frames;++i) for (unsigned c=0;c<8;++c) interleaved[i*8+c]=input[c][i];
    const int n=resampler_.ResampleOut(output_.data(),static_cast<int>(frames),static_cast<int>(capacity_),8);
    if (n<0 || n>static_cast<int>(capacity_)) return false;
    produced=static_cast<unsigned>(n);
    for (unsigned i=0;i<produced;++i) for (unsigned c=0;c<8;++c) output[c][i]=output_[i*8+c];
    return true;
}
double CinemaWdlSrc::latencySeconds() noexcept { return !rate_ || bypass() ? 0.0 : resampler_.GetCurrentLatency(); }
}
