#pragma once
#include "StageBusValidation.h"
#include <cstring>
namespace entropy::stage
{
enum class DemandResult
{
    Accepted,
    Duplicate,
    Fault
};
class StageBusDemand
{
  public:
    explicit StageBusDemand(const StageBusSessionHeader &s) : session_(s) {}
    DemandResult accept(const StageBusFrameRequest &r) noexcept
    {
        if (validateRequest(session_, r) != StageBusValidationResult::Ok || r.stream_epoch != 1)
            return DemandResult::Fault;
        if (seen_ && r.block_sequence == last_.block_sequence)
            return std::memcmp(&r, &last_, sizeof(r)) == 0 ? DemandResult::Duplicate
                                                           : DemandResult::Fault;
        if (pending_ || r.block_sequence != (seen_ ? last_.block_sequence + 1 : 1) ||
            (seen_ && r.control_revision < last_.control_revision))
            return DemandResult::Fault;
        last_ = r;
        seen_ = pending_ = true;
        return DemandResult::Accepted;
    }
    bool complete(uint64_t sequence) noexcept
    {
        if (!pending_ || sequence != last_.block_sequence)
            return false;
        pending_ = false;
        return true;
    }
    bool pending() const noexcept { return pending_; }

  private:
    StageBusSessionHeader session_{};
    StageBusFrameRequest last_{};
    bool seen_ = false, pending_ = false;
};
} // namespace entropy::stage
