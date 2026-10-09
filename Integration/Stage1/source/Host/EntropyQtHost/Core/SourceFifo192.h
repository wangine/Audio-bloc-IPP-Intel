#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

namespace entropy::host {

struct SourceStreamDescriptor {
    std::uint64_t generation = 0;
    std::uint32_t source_rate = 0;
    std::uint32_t channels = 0;
    std::uint32_t valid_bits = 0;
    std::uint32_t container_bits = 0;
    std::uint32_t format_revision = 0;

    bool operator==(const SourceStreamDescriptor&) const noexcept = default;
};

struct SourceFifo192Telemetry {
    std::uint32_t channels = 0;
    std::uint32_t capacity_frames = 0;
    std::uint32_t fill_frames = 0;
    std::uint32_t low_water_frames = 0;
    std::uint32_t high_water_frames = 0;
    std::uint64_t generation = 0;
    std::uint64_t produced_frames = 0;
    std::uint64_t consumed_frames = 0;
    std::uint64_t underruns = 0;
    std::uint64_t overflows = 0;
    std::uint64_t abandoned_frames = 0;
};

class SourceFifo192 final {
public:
    bool prepare(std::uint32_t channels, std::uint32_t capacityFrames);
    bool reset(const SourceStreamDescriptor& descriptor) noexcept;
    bool push(const double* const* inputs, std::uint32_t frames,
              const SourceStreamDescriptor& descriptor) noexcept;
    bool pop(double* const* outputs, std::uint32_t frames,
             SourceStreamDescriptor& descriptor) noexcept;

    // Compatibility adapters for call sites that only carry a generation.
    bool reset(std::uint64_t generation) noexcept;
    bool push(const double* const* inputs, std::uint32_t frames, std::uint64_t generation) noexcept;
    bool pop(double* const* outputs, std::uint32_t frames, std::uint64_t& generation) noexcept;
    SourceFifo192Telemetry telemetry() const noexcept;

private:
    // One producer owns push/reset; one consumer owns pop. Reset waits only
    // on the producer thread. The realtime consumer never waits for reset.
    struct Operation {
        explicit Operation(const SourceFifo192& owner) noexcept : owner(owner) {
            if (owner.resetting_.load()) return;
            owner.operations_.fetch_add(1);
            if (owner.resetting_.load()) owner.operations_.fetch_sub(1);
            else entered=true;
        }
        ~Operation() { if (entered) owner.operations_.fetch_sub(1); }
        const SourceFifo192& owner;
        bool entered=false;
    };
    mutable std::atomic<unsigned> operations_{0};
    std::atomic<bool> resetting_{false};
    std::vector<double> storage_;
    std::uint32_t channels_ = 0, capacity_ = 0;
    std::atomic<std::uint64_t> write_{0}, read_{0};
    SourceStreamDescriptor descriptor_{};
    std::atomic<std::uint64_t> produced_{0}, consumed_{0}, underruns_{0}, overflows_{0};
    std::atomic<std::uint64_t> abandoned_{0};
    std::atomic<std::uint32_t> lowWater_{0}, highWater_{0};
};

} // namespace entropy::host
