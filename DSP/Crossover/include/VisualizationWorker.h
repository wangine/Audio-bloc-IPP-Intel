#pragma once
#include "SpectrumView.h"
#include <thread>
namespace entropy::standalone {
class EntropyCrossoverAudioProcessor;
namespace visual {
class Worker final {
 EntropyCrossoverAudioProcessor& engine_;Source& source_;
 std::unique_ptr<Configuration> configuration_=std::make_unique<Configuration>();
 std::unique_ptr<SpectrumView> spectrum_=std::make_unique<SpectrumView>();
 FixedFFT inverse_;std::array<Ipp64fc,256> partition_{};
 std::array<double,FirKernel::maximumTaps> taps_{};
 std::array<float,fftSize> samples_{};
 mutable std::mutex frameMutex_;Frame frame_{};
 double qpcFrequency_=1;std::atomic<bool> stop_{false};std::thread owner_;
 bool response(Frame&,uint64_t descriptor);void run()noexcept;
public:
 explicit Worker(EntropyCrossoverAudioProcessor&);~Worker();
 Frame snapshot()const;
};
}
}
