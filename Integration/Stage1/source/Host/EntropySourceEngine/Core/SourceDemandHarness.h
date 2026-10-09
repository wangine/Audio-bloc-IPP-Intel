#pragma once
#include "StageBusValidation.h"
#include <cstring>
namespace entropy::host
{
enum class SourceDemandOwner
{
    None,
    Standalone,
    External
};
class SourceDemandHarness
{
  public:
    bool begin(const StageBusSessionHeader &s, SourceDemandOwner owner) noexcept;
    bool next(uint64_t revision, StageBusFrameRequest &) noexcept;
    bool complete(uint64_t sequence) noexcept;
    bool quiesce() noexcept;
    SourceDemandOwner owner() const noexcept { return owner_; }

  private:
    StageBusSessionHeader session_{};
    SourceDemandOwner owner_ = SourceDemandOwner::None;
    uint64_t sequence_ = 0;
    bool pending_ = false;
};
} // namespace entropy::host
