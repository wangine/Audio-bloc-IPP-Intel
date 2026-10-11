#pragma once
#include "endpoint_provider_v2.h"
#include <cstddef>
#include <span>
namespace entropy::music {
struct IngressPacketView {
    const EntropyIngressPacketV2 &header;
    std::span<const std::byte> payload;
};
enum class DecodeStatus { Ok, Invalid, Unsupported, Unimplemented };
struct DecodeResult {
    DecodeStatus status;
    uint32_t frames;
    explicit operator bool() const { return status == DecodeStatus::Ok; }
};
bool validFormat(const EntropyIngressFormatV2 &) noexcept;
DecodeResult decodeStereo(const IngressPacketView &, double *, double *, uint32_t) noexcept;
} // namespace entropy::music
