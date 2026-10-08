#include "BufferPool.h"
#include <algorithm>
#include <stdexcept>

namespace entropy::engine {
void PlanarBuffer::prepare(std::uint32_t channels, std::uint32_t frames) {
    if (channels == 0 || channels > kMaximumChannels || frames == 0 || frames > 65536)
        throw std::invalid_argument("Invalid planar buffer dimensions");
    channels_ = channels;
    capacity_ = frames;
    data_.assign(static_cast<std::size_t>(channels) * frames, 0.0);
    write_.fill(nullptr);
    read_.fill(nullptr);
    for (std::uint32_t ch = 0; ch < channels; ++ch) {
        write_[ch] = data_.data() + static_cast<std::size_t>(ch) * frames;
        read_[ch] = write_[ch];
    }
}
void PlanarBuffer::clear(std::uint32_t frames) noexcept {
    const auto count = std::min(frames, capacity_);
    for (std::uint32_t ch = 0; ch < channels_; ++ch)
        std::fill_n(write_[ch], count, 0.0);
}
}
