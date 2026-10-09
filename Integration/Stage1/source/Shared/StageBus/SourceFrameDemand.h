#pragma once
#include "StageBusDemand.h"
namespace entropy::stage
{
// In-process Source boundary, not a new wire struct.
struct SourceFrameEnvelope
{
    uint8_t session_id[16]{};
    uint64_t stream_epoch=0, block_sequence=0, first_frame=0, control_revision=0;
    uint32_t frames=0;
    explicit SourceFrameEnvelope(const StageBusFrameRequest& r) noexcept
        : stream_epoch(r.stream_epoch), block_sequence(r.block_sequence), first_frame(r.first_frame),
          control_revision(r.control_revision), frames(r.frames)
    { std::memcpy(session_id,r.session_id,16); }
    bool operator==(const SourceFrameEnvelope& r) const noexcept
    {
        return !std::memcmp(session_id,r.session_id,16) && stream_epoch==r.stream_epoch &&
               block_sequence==r.block_sequence && first_frame==r.first_frame &&
               control_revision==r.control_revision && frames==r.frames;
    }
};
class SourceFrameDemand
{
public:
    explicit SourceFrameDemand(const StageBusSessionHeader& s):session_(s){}
    DemandResult accept(const StageBusFrameRequest& r) noexcept
    {
        if(validateSourceDemandRequest(session_,r)!=StageBusValidationResult::Ok)
            return DemandResult::Fault;
        SourceFrameEnvelope next(r);
        if(seen_ && next.block_sequence==last_.block_sequence)
            return next==last_?DemandResult::Duplicate:DemandResult::Fault;
        if(pending_ || next.block_sequence!=(seen_?last_.block_sequence+1:1) ||
           (seen_ && next.control_revision<last_.control_revision)) return DemandResult::Fault;
        last_=next; seen_=pending_=true; return DemandResult::Accepted;
    }
    bool complete(uint64_t seq) noexcept
    { if(!pending_ || seq!=last_.block_sequence)return false;pending_=false;return true; }
    bool pending() const noexcept{return pending_;}
private:
    StageBusSessionHeader session_{};
    SourceFrameEnvelope last_{StageBusFrameRequest{}};
    bool seen_=false,pending_=false;
};
}
