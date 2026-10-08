#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

namespace entropy::egress {
// B2-FF-001 KEEP_MINIMAL candidate, acceptance OPEN. Registry: docs/Beta2/first-fault-registry.md.
// One winning writer per lifecycle. Readers never touch payload until READY.
// resetQuiescent/destroy require ALL producer/consumer/observer calls stopped.
// CAS order is arbitration order, NOT a physical ordering of simultaneous faults.
template<class Record> class FirstFaultSlot final {
    static_assert(std::is_trivially_copyable_v<Record>);
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
public:
    enum : std::uint32_t { Empty=0, Writing=1, Ready=2 };
    // Internal publication token, not exported in C ABI. A harness can hold this
    // ordinary token to test a delayed writer without any runtime injection hook.
    // A claimed token must be published; abandonment intentionally stays WRITING.
    class Writer final {
        friend class FirstFaultSlot;
        explicit Writer(FirstFaultSlot* slot) noexcept : slot_(slot) {}
        FirstFaultSlot* slot_{};
    public:
        Writer(const Writer&)=delete;
        Writer& operator=(const Writer&)=delete;
        Writer(Writer&& other) noexcept : slot_(std::exchange(other.slot_,nullptr)) {}
        explicit operator bool() const noexcept { return slot_!=nullptr; }
        bool publish(const Record& record) noexcept {
            if(!slot_)return false;
            std::memcpy(&slot_->record_,&record,sizeof(Record));
            slot_->state_.store(Ready,std::memory_order_release);
            slot_=nullptr;
            return true;
        }
    };
    Writer tryBeginCapture() noexcept {
        std::uint32_t expected=Empty;
        if(!state_.compare_exchange_strong(expected,Writing,
            std::memory_order_acq_rel,std::memory_order_acquire))return Writer{nullptr};
        return Writer{this};
    }
    bool tryCapture(const Record& record) noexcept {
        auto writer=tryBeginCapture();
        return writer.publish(record);
    }
    bool read(Record& result) const noexcept {
        if(state_.load(std::memory_order_acquire)!=Ready)return false;
        std::memcpy(&result,&record_,sizeof(Record));
        return true;
    }
    std::uint32_t state() const noexcept { return state_.load(std::memory_order_acquire); }
    void resetQuiescent() noexcept {
        record_={};
        state_.store(Empty,std::memory_order_release);
    }
private:
    std::atomic<std::uint32_t> state_{Empty};
    Record record_{};
};
} // namespace entropy::egress
