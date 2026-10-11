#pragma once
#include <cstdint>
namespace entropy::music::audit {
struct PauseEvidence {
    uint64_t clockBefore=0,clockAfter=0,sourceBefore=0,sourceAfter=0,generationBefore=0,generationAfter=0;
    uint32_t observed=0;
    bool valid() const noexcept {
        return observed==1 && generationBefore && generationBefore==generationAfter &&
            clockBefore==clockAfter && sourceBefore==sourceAfter;
    }
};
}
