#pragma once
#include <cstdint>
namespace entropy::music::audit {
inline bool completeOutputBoundary(bool producerExited,uint32_t exitCode,uint64_t read,uint64_t write,uint64_t drops) noexcept {
    return producerExited && exitCode==0 && read==write && !drops;
}
inline bool completeFp64Tail(uint32_t rate,uint64_t frames) noexcept {return rate==192000 && frames && frames%1024==0;}
}
