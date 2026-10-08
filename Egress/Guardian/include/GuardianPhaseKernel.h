#pragma once

namespace entropy::guardian::detail
{
// Internal implementation detail. No ABI, quality or DSP policy change.
// Only call the AVX2 entry after hasAvx2() has checked CPU and OS support.
bool hasAvx2() noexcept;
void reconstruct16Avx2(const double* samples256,
                       const double* coefficientsPhase4,
                       double* phaseOutputs16) noexcept;
}
