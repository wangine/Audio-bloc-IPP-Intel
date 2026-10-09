#include "Core/RealtimeBufferPolicy.h"

#include <cstdlib>
#include <iostream>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
}

int main()
{
    using entropy::host::RealtimeBufferPolicy;

    require(RealtimeBufferPolicy::sourcePrerollFrames == 8192,
            "Reference A source preroll must be 42.667 ms at 192 kHz");
    require(RealtimeBufferPolicy::sourceTargetFrames == 16384,
            "Reference A source target must be 85.333 ms at 192 kHz");
    require(RealtimeBufferPolicy::egressTargetFrames == 16384,
            "Reference A ClockSync target must be 85.333 ms at 192 kHz");
    require(RealtimeBufferPolicy::sourceCapacityFrames == 524288 &&
            RealtimeBufferPolicy::sourceCaptureCapacityFrames == 131072 &&
            RealtimeBufferPolicy::egressCapacityFrames == 65536,
            "latency reduction must preserve reserve capacities");
    require(RealtimeBufferPolicy::sourceTargetFrames >= RealtimeBufferPolicy::sourcePrerollFrames,
            "source target must cover startup preroll");
    require(RealtimeBufferPolicy::sourceTargetFrames + 1024 <
                RealtimeBufferPolicy::sourceCapacityFrames,
            "source target must leave explicit FIFO headroom");
    require(RealtimeBufferPolicy::egressTargetFrames + 8192 + 100 <
                RealtimeBufferPolicy::egressCapacityFrames,
            "egress target must leave maximum render block plus 96-tap ASRC guard + 4");

    std::cout << "PASS bounded source/egress latency policy\n";
    return 0;
}
