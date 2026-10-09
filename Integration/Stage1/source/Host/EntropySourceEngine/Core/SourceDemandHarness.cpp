#include "SourceDemandHarness.h"
namespace entropy::host
{
bool SourceDemandHarness::begin(const StageBusSessionHeader &session,
                                SourceDemandOwner owner) noexcept
{
    if (owner_ != SourceDemandOwner::None || pending_ || owner == SourceDemandOwner::None ||
        entropy::stage::validateSession(session, session.mapping_bytes) !=
            entropy::stage::StageBusValidationResult::Ok ||
        std::memcmp(session_.session_id, session.session_id, 16) == 0)
        return false;
    session_ = session;
    owner_ = owner;
    sequence_ = 0;
    return true;
}
bool SourceDemandHarness::next(uint64_t revision,
                               StageBusFrameRequest &out) noexcept
{
    if (owner_ != SourceDemandOwner::Standalone || pending_ || sequence_ >= UINT64_MAX / 1024)
        return false;
    StageBusFrameRequest r{};
    std::memcpy(r.session_id, session_.session_id, 16);
    r.stream_epoch = 1;
    r.block_sequence = sequence_ + 1;
    r.first_frame = sequence_ * 1024;
    r.frames = 1024;
    r.control_revision = revision;
    if (entropy::stage::validateSourceDemandRequest(session_, r) !=
        entropy::stage::StageBusValidationResult::Ok)
        return false;
    out = r;
    ++sequence_;
    pending_ = true;
    return true;
}
bool SourceDemandHarness::complete(uint64_t seq) noexcept
{
    if (!pending_ || seq != sequence_)
        return false;
    pending_ = false;
    return true;
}
bool SourceDemandHarness::quiesce() noexcept
{
    if (pending_)
        return false;
    owner_ = SourceDemandOwner::None;
    return true;
}
} // namespace entropy::host
