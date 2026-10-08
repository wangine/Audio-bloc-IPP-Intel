#include "GuardianPhaseKernel.h"
#include <immintrin.h>

namespace entropy::guardian::detail
{
// Four independent phases in SIMD lanes. Each phase preserves the original
// eight partial sums and their SSE2 reduction tree exactly; deliberately no FMA.
void reconstruct16Avx2(const double* samples,
                       const double* coefficients,
                       double* outputs) noexcept
{
    for (int group = 0; group < 4; ++group)
    {
        const auto* k = coefficients + group * 256 * 4;
        __m256d s0 = _mm256_setzero_pd(), s1 = s0, s2 = s0, s3 = s0;
        __m256d s4 = s0, s5 = s0, s6 = s0, s7 = s0;
        for (int tap = 0; tap < 256; tap += 8)
        {
            s0 = _mm256_add_pd(s0, _mm256_mul_pd(_mm256_broadcast_sd(samples + tap),
                                               _mm256_loadu_pd(k + 4 * tap)));
            s1 = _mm256_add_pd(s1, _mm256_mul_pd(_mm256_broadcast_sd(samples + tap + 1),
                                               _mm256_loadu_pd(k + 4 * (tap + 1))));
            s2 = _mm256_add_pd(s2, _mm256_mul_pd(_mm256_broadcast_sd(samples + tap + 2),
                                               _mm256_loadu_pd(k + 4 * (tap + 2))));
            s3 = _mm256_add_pd(s3, _mm256_mul_pd(_mm256_broadcast_sd(samples + tap + 3),
                                               _mm256_loadu_pd(k + 4 * (tap + 3))));
            s4 = _mm256_add_pd(s4, _mm256_mul_pd(_mm256_broadcast_sd(samples + tap + 4),
                                               _mm256_loadu_pd(k + 4 * (tap + 4))));
            s5 = _mm256_add_pd(s5, _mm256_mul_pd(_mm256_broadcast_sd(samples + tap + 5),
                                               _mm256_loadu_pd(k + 4 * (tap + 5))));
            s6 = _mm256_add_pd(s6, _mm256_mul_pd(_mm256_broadcast_sd(samples + tap + 6),
                                               _mm256_loadu_pd(k + 4 * (tap + 6))));
            s7 = _mm256_add_pd(s7, _mm256_mul_pd(_mm256_broadcast_sd(samples + tap + 7),
                                               _mm256_loadu_pd(k + 4 * (tap + 7))));
        }
        const auto even = _mm256_add_pd(_mm256_add_pd(s0, s2), _mm256_add_pd(s4, s6));
        const auto odd  = _mm256_add_pd(_mm256_add_pd(s1, s3), _mm256_add_pd(s5, s7));
        _mm256_storeu_pd(outputs + group * 4, _mm256_add_pd(even, odd));
    }
}
}
