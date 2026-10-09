#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace entropy::host
{
struct RealtimeControlCommand final
{
    std::uint64_t sequence = 0;
    std::uint32_t kind = 0;
    std::uint32_t role = 0;
    std::uint32_t index = 0;
    std::uint32_t reserved = 0;
    std::array<double, 4> payload{};
};

static_assert(std::is_trivially_copyable_v<RealtimeControlCommand>);

class RealtimeControlQueue final
{
public:
    static constexpr std::size_t capacity = 64;

    bool tryPush(const RealtimeControlCommand& command) noexcept
    {
        const auto write = writeIndex_.value.load(std::memory_order_relaxed);
        const auto read = readIndex_.value.load(std::memory_order_acquire);
        if (write - read >= capacity) return false;

        storage_[write % capacity] = command;
        writeIndex_.value.store(write + 1, std::memory_order_release);
        return true;
    }

    bool tryPop(RealtimeControlCommand& command) noexcept
    {
        const auto read = readIndex_.value.load(std::memory_order_relaxed);
        const auto write = writeIndex_.value.load(std::memory_order_acquire);
        if (read == write) return false;

        command = storage_[read % capacity];
        readIndex_.value.store(read + 1, std::memory_order_release);
        return true;
    }

private:
    struct alignas(64) PublishedIndex final
    {
        std::atomic<std::uint64_t> value{0};
    };

    std::array<RealtimeControlCommand, capacity> storage_{};
    PublishedIndex writeIndex_{};
    PublishedIndex readIndex_{};

    static_assert(alignof(PublishedIndex) >= 64);
    static_assert(sizeof(PublishedIndex) >= 64);
};
}
