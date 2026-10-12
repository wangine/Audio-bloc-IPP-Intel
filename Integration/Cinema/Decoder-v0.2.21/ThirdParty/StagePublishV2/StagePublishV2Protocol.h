#pragma once
#include <entropy/stage_bus_v1.h>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstring>
namespace entropy::publish::wire {
inline constexpr std::uint64_t magic=0x3256425550475453ULL;
inline constexpr std::uint32_t version=2, capacity=16, subscribers=4;
struct Request {
    std::uint64_t magic=wire::magic;
    std::uint32_t version=wire::version,bytes=sizeof(Request),clientPid=0,reserved=0;
    std::uint8_t nonce[16]{};
};
enum class Status:std::uint32_t {Ok,Waiting,Full,Invalid};
struct Response {
    std::uint64_t magic=wire::magic;
    std::uint32_t version=wire::version,bytes=sizeof(Response),publisherPid=0;
    Status status=Status::Waiting;
    std::uint8_t nonce[16]{},token[16]{};
    wchar_t mapping[160]{};
    StageBusSessionHeader session{};
    std::uint64_t reserved[2]{};
};
struct Slot { StageBusBlockHeader header{}; double planes[11][1024]{}; };
struct alignas(64) Queue {
    std::uint64_t magic=wire::magic;
    std::uint32_t version=wire::version,bytes=sizeof(Queue),capacity=wire::capacity,publisherPid=0;
    std::uint8_t token[16]{};
    StageBusSessionHeader session{};
    alignas(64) std::atomic<std::uint64_t> write{0};
    alignas(64) std::atomic<std::uint64_t> read{0};
    alignas(64) std::atomic<std::uint64_t> drops{0};
    alignas(64) std::atomic<std::uint64_t> heartbeat{0};
    alignas(64) std::atomic<std::uint32_t> connected{1};
    alignas(64) Slot slots[wire::capacity];
};
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
static_assert(sizeof(wchar_t)==2);
static_assert(sizeof(Request)==40 && sizeof(Response)==648);
static_assert(sizeof(Slot)==90240 && sizeof(Queue)==1444480);
static_assert(offsetof(Queue,session)==40 && offsetof(Queue,write)==320);
static_assert(offsetof(Queue,read)==384 && offsetof(Queue,drops)==448);
static_assert(offsetof(Queue,heartbeat)==512 && offsetof(Queue,connected)==576);
static_assert(offsetof(Queue,slots)==640);
inline bool zero(const void* p,std::size_t n) noexcept {
    auto* b=static_cast<const std::uint8_t*>(p);std::uint8_t value=0;
    for(std::size_t i=0;i<n;i++)value|=b[i];return value==0;
}
inline bool validRequest(const Request& r,std::uint32_t verifiedPid) noexcept {
    return r.magic==magic && r.version==version && r.bytes==sizeof(r) &&
        r.clientPid && r.clientPid==verifiedPid && !r.reserved && !zero(r.nonce,16);
}
}
