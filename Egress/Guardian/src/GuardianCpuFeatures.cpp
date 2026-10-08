#include "GuardianPhaseKernel.h"
#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace entropy::guardian::detail
{
bool hasAvx2() noexcept
{
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    int data[4]{};
    __cpuid(data, 0);
    if (data[0] < 7) return false;
    __cpuidex(data, 1, 0);
    constexpr int required = (1 << 26) | (1 << 27) | (1 << 28);
    if ((data[2] & required) != required) return false;
    if ((_xgetbv(0) & 6u) != 6u) return false;
    __cpuidex(data, 7, 0);
    return (data[1] & (1 << 5)) != 0;
#elif (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__i386__))
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx2");
#else
    return false;
#endif
}
}
