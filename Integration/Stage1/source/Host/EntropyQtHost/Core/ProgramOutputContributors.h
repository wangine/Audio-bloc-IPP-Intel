#pragma once
#include <entropy/output_matrix_abi.h>
namespace entropy::host {
inline uint32_t programOutputContributors(const EntropyOutputMatrixState& state) noexcept {
    uint32_t mask=0;
    for(uint32_t source=0;source<ENTROPY_OUTPUT_MATRIX_CHANNELS;++source)
        if((state.active_source_mask&(1u<<source))!=0 &&
           (state.source_destination_mask[source]&state.enabled_destination_mask)!=0)
            mask|=1u<<source;
    return mask;
}
}
