#pragma once
#include "EntropyBandlimitedAsrc.h"
#include "../ProgramChain/MatrixReferenceCore.h"
#include <emmintrin.h>
#include <type_traits>
#include <vector>

namespace entropy::asrc {
// Prepared outside the callback. Matrix each source frame once, then convolve
// contiguous planar doubles. The 96-tap filter, phase and gain math are unchanged.
class SincMatrix {
    std::vector<double> planar_;
public:
    void prepare(std::size_t ringFrames, int channels) {
        planar_.resize((ringFrames + PolyphaseSinc::kTaps + 2) * channels);
    }
    static double dot(const double* samples, const double* kernel) noexcept {
        __m128d a = _mm_setzero_pd(), b = a, c = a, d = a;
        for (int i = 0; i < PolyphaseSinc::kTaps; i += 8) {
            a = _mm_add_pd(a, _mm_mul_pd(_mm_loadu_pd(samples+i), _mm_loadu_pd(kernel+i)));
            b = _mm_add_pd(b, _mm_mul_pd(_mm_loadu_pd(samples+i+2), _mm_loadu_pd(kernel+i+2)));
            c = _mm_add_pd(c, _mm_mul_pd(_mm_loadu_pd(samples+i+4), _mm_loadu_pd(kernel+i+4)));
            d = _mm_add_pd(d, _mm_mul_pd(_mm_loadu_pd(samples+i+6), _mm_loadu_pd(kernel+i+6)));
        }
        const auto sum = _mm_add_pd(_mm_add_pd(a,b),_mm_add_pd(c,d));
        double lanes[2]; _mm_storeu_pd(lanes,sum); return lanes[0]+lanes[1];
    }
    template <typename RingSample>
    bool render(const RingSample* ring, std::size_t capacity, unsigned sources,
                double* output, unsigned frames, int channels, const double* gains,
                double& position, double step) noexcept {
        if (!ring || !output || !gains || !frames || channels < 1 || channels > 16 ||
            !sources || sources > 16 || capacity == 0 || (capacity & (capacity-1)) ||
            !std::isfinite(position) || !std::isfinite(step) || step <= 0) return false;
        const auto first = static_cast<std::int64_t>(std::floor(position)) + PolyphaseSinc::kFirstOffset;
        // Two guard frames cover rounding differences between multiplication
        // here and the callback's repeated position additions below.
        const auto last = static_cast<std::int64_t>(std::floor(position+(frames-1)*step)) + PolyphaseSinc::kLastOffset + 2;
        const auto span = static_cast<std::size_t>(last-first+1);
        if (span > capacity + PolyphaseSinc::kTaps + 2 || span*channels > planar_.size()) return false;
        std::array<int,16> counts {};
        std::array<std::array<unsigned,16>,16> indices {};
        std::array<std::array<double,16>,16> weights {};
        for (int ch=0;ch<channels;++ch) {
            for(unsigned s=0;s<sources;++s) if(gains[ch*sources+s]!=0) {
                const auto i=counts[ch]++; indices[ch][i]=s; weights[ch][i]=gains[ch*sources+s];
            }
            if(!counts[ch]) continue;
            auto* plane=planar_.data()+ch*span;
            for(std::size_t f=0;f<span;++f) {
                const auto* sample=ring+((static_cast<std::uint64_t>(first+f))&(capacity-1))*sources;
                entropy::matrix::NeumaierAccumulator mixed;
                for(int i=0;i<counts[ch];++i) {
                    double value=static_cast<double>(sample[indices[ch][i]]);
                    if constexpr(std::is_same_v<RingSample,std::int32_t>)
                        value*=1.0/2147483648.0;
                    mixed.add(value*weights[ch][i]);
                }
                plane[f]=mixed.result();
            }
        }
        for(unsigned f=0;f<frames;++f) {
            const auto lower=static_cast<std::int64_t>(std::floor(position));
            const auto phase=PolyphaseSinc::phaseFor(position-lower);
            std::array<double,PolyphaseSinc::kTaps> kernel;
            for(int t=0;t<PolyphaseSinc::kTaps;++t) kernel[t]=PolyphaseSinc::coefficient(phase,t);
            const auto offset=static_cast<std::size_t>(lower+PolyphaseSinc::kFirstOffset-first);
            for(int ch=0;ch<channels;++ch)
                output[f*channels+ch]=counts[ch] ? dot(planar_.data()+ch*span+offset,kernel.data()) : 0;
            position+=step;
        }
        return true;
    }
};
}
