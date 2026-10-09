#pragma once
#include "StageBusValidation.h"
#include <cstdint>
#include <string>
namespace entropy::stage
{
enum class StageBusIoResult
{
    Ok,
    Full,
    Empty,
    Fault
};
class StageBusWin32
{
  public:
    StageBusWin32() = default;
    ~StageBusWin32();
    StageBusWin32(const StageBusWin32 &) = delete;
    StageBusWin32 &operator=(const StageBusWin32 &) = delete;
    bool create(const StageBusSessionHeader &, std::string &error);
    bool open(const StageBusSessionHeader &, std::string &error, void *preferredAddress = nullptr);
    void close() noexcept;
    bool writable() const noexcept;
    bool requestPending() const noexcept;
    StageBusIoResult tryPublish(const StageBusBlockHeader &,
                                const double *const *channels) noexcept;
    StageBusIoResult tryConsume(StageBusBlockHeader &, double *const *channels) noexcept;
    StageBusIoResult trySendRequest(const StageBusFrameRequest &) noexcept;
    StageBusIoResult tryReceiveRequest(StageBusFrameRequest &) noexcept;
    StageBusIoResult trySendResponse(const StageBusBlockHeader &) noexcept;
    StageBusIoResult tryReceiveResponse(StageBusBlockHeader &) noexcept;
    StageBusIoResult trySendSourceControl(const StageBusSourceControl &) noexcept;
    StageBusIoResult tryReceiveSourceControl(StageBusSourceControl &) noexcept;
    StageBusIoResult trySendSourceRequest(const StageBusFrameRequest &) noexcept;
    StageBusIoResult tryReceiveSourceRequest(StageBusFrameRequest &) noexcept;
    void heartbeat(uint64_t qpc) noexcept;
    bool peerAlive(uint64_t now) const noexcept;
    void fault() noexcept;
    void *address() const noexcept { return view_; }
    uint64_t fullCount() const noexcept { return full_; }
    uint64_t emptyCount() const noexcept { return empty_; }

  private:
    void *mapping_ = nullptr;
    void *event_ = nullptr;
    void *peer_ = nullptr;
    void *view_ = nullptr;
    bool producer_ = false;
    StageBusSessionHeader session_{};
    uint64_t lastSequence_ = 0, lastEpoch_ = 0, full_ = 0, empty_ = 0;
};
} // namespace entropy::stage
