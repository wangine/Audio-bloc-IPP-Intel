#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace entropy::engine {
class PlanarBuffer final {
public:
    static constexpr std::uint32_t kMaximumChannels = 16;
    void prepare(std::uint32_t channels, std::uint32_t maximumFrames);
    void clear(std::uint32_t frames) noexcept;
    double* channel(std::uint32_t ch) noexcept { return write_[ch]; }
    const double* readChannel(std::uint32_t ch) const noexcept { return read_[ch]; }
    double* const* writePointers() const noexcept { return write_.data(); }
    const double* const* readPointers() const noexcept { return read_.data(); }
    std::uint32_t channels() const noexcept { return channels_; }
    std::uint32_t capacity() const noexcept { return capacity_; }
private:
    std::vector<double> data_;
    std::array<double*, kMaximumChannels> write_{};
    std::array<const double*, kMaximumChannels> read_{};
    std::uint32_t channels_ = 0;
    std::uint32_t capacity_ = 0;
};
}
