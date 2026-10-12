#pragma once
#include "../model/Types.h"
namespace entropy::music {
// No elapsed-time-only PCM certification. Position allocations remain a pure
// offline research model; production rejects them until OS ownership is proven.
constexpr bool rawAllocationSupported(uint32_t notifications) noexcept{return notifications==1 || notifications==2;}
constexpr uint32_t minimumPacketFrames(uint32_t rate) noexcept{return (rate+999)/1000;}
}
