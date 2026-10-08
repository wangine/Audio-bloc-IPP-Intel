#pragma once
#include <cstdint>
// Protocol declarations only. An IPC transport/server is NOT running in Phase 01.
// Qt never shares C++ object pointers or calls a DSP callback directly.
namespace entropy::ipc {
inline constexpr std::uint32_t kProtocolVersion = 1;
enum class Command : std::uint32_t {
    GetDescriptor = 1, SetParameter = 2, GetParameter = 3,
    GetTelemetry = 4, LoadProfile = 5, SaveProfile = 6,
    Arm = 7, Disarm = 8, SetOutputRouting = 9
};
struct Header {
    std::uint32_t version = kProtocolVersion;
    std::uint32_t bytes = 0;
    std::uint32_t command = 0;
    std::uint32_t requestId = 0;
};
static_assert(sizeof(Header) == 16);
}
