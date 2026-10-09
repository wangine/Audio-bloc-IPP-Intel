#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace entropy::host
{
inline std::string describeInputTransport(std::string_view providerId,
                                          std::uint32_t validBits,
                                          std::uint32_t containerBits)
{
    if (providerId == "audio.entropy.endpoint.asio-ingress")
        return "Transport: ASIO Float32; track bit depth unavailable";
    if (validBits == 0 || containerBits == 0)
        return "Transport resolution: unavailable";
    return "Transport: " + std::to_string(validBits) + " valid / " +
           std::to_string(containerBits) + " container";
}
} // namespace entropy::host
