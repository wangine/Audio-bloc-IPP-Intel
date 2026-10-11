#pragma once
#include "../model/Format.h"
namespace entropy::music::audit {
struct PcmPattern {static void frame(uint64_t index,const PcmFormat&,uint8_t* bytes) noexcept;};
}
