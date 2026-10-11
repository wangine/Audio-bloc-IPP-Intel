#pragma once
#include "RealtimeFir.h"
#include <cmath>
namespace entropy::native {
// Passive control/UI spectrum only. Convert the monitor floats to FP64 before
// the transform; audio convolution never uses a float FFT. Construction prepares
// and warms all storage. Calls on an instance must be serialized by its owner.
class IppSpectrum {
    IppComplexFft fft;
    std::vector<Ipp64fc> frame;
public:
    explicit IppSpectrum(int order):fft(order),frame(static_cast<std::size_t>(fft.size())){}
    void magnitudes(const double* input,double* out) noexcept {
        for(int i=0;i<size();++i)frame[i]={input[i],0};
        fft.run(frame.data());
        for(int i=0;i<=size()/2;++i)out[i]=std::hypot(frame[i].re,frame[i].im);
    }
    int size() const noexcept { return fft.size(); }
    // out has size()/2+1 entries, including DC and Nyquist. These are raw
    // unnormalized FFT magnitudes, preserving the existing viewer convention.
    void magnitudes(const float* input,double* out) noexcept {
        for(int i=0;i<size();++i)frame[i]={static_cast<double>(input[i]),0};
        fft.run(frame.data());
        for(int i=0;i<=size()/2;++i)out[i]=std::hypot(frame[i].re,frame[i].im);
    }
};
}


