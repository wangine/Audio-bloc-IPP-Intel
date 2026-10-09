#pragma once
#include <immintrin.h>
namespace entropy::host {
// x64 audio callback policy. Subnormal operands/results are treated as zero
// only inside this scope. Rounding and all normal FP64 values are unchanged.
// Preserve the caller's full MXCSR state on exit; never change another thread.
class AudioFloatingPointScope final {
public:
    AudioFloatingPointScope() noexcept : saved_(_mm_getcsr()) { _mm_setcsr(saved_|0x8040u); }
    ~AudioFloatingPointScope() { _mm_setcsr(saved_); }
    AudioFloatingPointScope(const AudioFloatingPointScope&)=delete;
    AudioFloatingPointScope& operator=(const AudioFloatingPointScope&)=delete;
private:
    unsigned saved_;
};
}
