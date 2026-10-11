#include "StageBusValidation.h"
#include "StageBusHashes.h"
#include <cstring>
#include <limits>
namespace entropy::stage
{
namespace
{
bool zero(const void *p, size_t n) noexcept
{
    auto *b = static_cast<const uint8_t *>(p);
    for (size_t i = 0; i < n; ++i)
        if (b[i])
            return false;
    return true;
}
bool identity(const StageBusSessionHeader &s, const uint8_t *uuid, uint64_t epoch,
              uint64_t sequence, uint64_t first, uint64_t revision, uint32_t frames) noexcept
{
    return std::memcmp(s.session_id, uuid, 16) == 0 && epoch != 0 && revision != 0 &&
           frames == 1024 && sequence != 0 && sequence <= UINT64_MAX / 1024 &&
           first == (sequence - 1) * 1024;
}
} // namespace
StageBusSessionHeader makeSession(uint32_t layout, const uint8_t *uuid, uint64_t qpc) noexcept
{
    StageBusSessionHeader s{};
    s.magic = STAGE_BUS_MAGIC;
    s.abi_version = STAGE_BUS_ABI;
    s.header_bytes = sizeof(s);
    if (uuid)
        std::memcpy(s.session_id, uuid, 16);
    s.layout_id = layout;
    s.channels = layout == MIX_CINEMA10_V1 ? 10 : layout == LRC_CINEMA11_V1 ? 11 :
                 layout == MUSIC_STEREO2_V1 ? 2 : layout == MUSIC_LRC3_V1 ? 3 : 0;
    s.sample_rate = 192000;
    s.sample_format = STAGE_BUS_BINARY64_LE;
    s.planar = STAGE_BUS_PLANAR;
    s.frames_per_block = 1024;
    s.capacity = 4;
    s.block_header_bytes = 128;
    s.plane_stride = 1024 * 8;
    s.slot_stride = 128 + s.channels * s.plane_stride;
    s.slots_offset = 4096;
    s.mapping_bytes = s.slots_offset + 4 * s.slot_stride;
    s.qpc_frequency = qpc;
    const bool music = layout == MUSIC_STEREO2_V1 || layout == MUSIC_LRC3_V1;
    std::memcpy(s.contract_hash, music ? musicContractHash : contractHash, 32);
    const auto hash = layout == MUSIC_STEREO2_V1 ? layoutHash2 : layout == MUSIC_LRC3_V1 ? layoutHash3 :
                      layout == MIX_CINEMA10_V1 ? layoutHash10 : layoutHash11;
    std::memcpy(s.layout_hash, hash, 32);
    return s;
}
StageBusValidationResult validateSession(const StageBusSessionHeader &s, size_t mapped) noexcept
{
    const auto expected = makeSession(s.layout_id, s.session_id, s.qpc_frequency);
    if (s.magic != STAGE_BUS_MAGIC || s.abi_version != STAGE_BUS_ABI || s.header_bytes != 256 ||
        expected.channels == 0 || s.channels != expected.channels || s.sample_rate != 192000 ||
        s.sample_format != 1 || s.planar != 1 || s.frames_per_block != 1024 || s.capacity != 4 ||
        s.block_header_bytes != 128 || s.qpc_frequency == 0 ||
        !zero(s.reserved, sizeof(s.reserved)) || std::memcmp(s.contract_hash, expected.contract_hash, 32) ||
        std::memcmp(s.layout_hash, expected.layout_hash, 32))
        return StageBusValidationResult::Incompatible;
    if (zero(s.session_id, 16))
        return StageBusValidationResult::Identity;
    if (s.plane_stride != expected.plane_stride || s.slot_stride != expected.slot_stride ||
        s.slots_offset != expected.slots_offset || s.mapping_bytes != expected.mapping_bytes ||
        mapped < s.mapping_bytes)
        return StageBusValidationResult::Bounds;
    return StageBusValidationResult::Ok;
}
StageBusValidationResult validateBlock(const StageBusSessionHeader &s, const StageBusBlockHeader &b,
                                       size_t slot) noexcept
{
    if (validateSession(s, s.mapping_bytes) != StageBusValidationResult::Ok ||
        slot != s.slot_stride)
        return StageBusValidationResult::Bounds;
    if (!identity(s, b.session_id, b.stream_epoch, b.block_sequence, b.first_frame,
                  b.control_revision, b.frames))
        return StageBusValidationResult::Identity;
    if ((b.validity_mask | b.discontinuity_mask) & ~7u || b.reserved)
        return StageBusValidationResult::Incompatible;
    if ((s.layout_id == MUSIC_STEREO2_V1 || s.layout_id == MUSIC_LRC3_V1) &&
        (((b.validity_mask | b.discontinuity_mask) & ~3u) || b.source_generation[2]))
        return StageBusValidationResult::Incompatible;
    return StageBusValidationResult::Ok;
}
StageBusValidationResult validateRequest(const StageBusSessionHeader &s,
                                         const StageBusFrameRequest &r) noexcept
{
    if (!identity(s, r.session_id, r.stream_epoch, r.block_sequence, r.first_frame,
                  r.control_revision, r.frames))
        return StageBusValidationResult::Identity;
    const auto &p = r.program;
    if (r.reserved || p.program > 1 || !p.required_inputs || (p.required_inputs & ~3u) ||
        !(p.required_inputs & (p.program ? 2u : 1u)) || p.music_program_active > 1 ||
        p.analyze_music_final > 1 || (p.manual_mute_mask | p.effective_mute_mask) & ~7u ||
        p.hardware_armed > 1 || p.output_source_mask & ~0xffffu)
        return StageBusValidationResult::Incompatible;
    return StageBusValidationResult::Ok;
}
} // namespace entropy::stage
