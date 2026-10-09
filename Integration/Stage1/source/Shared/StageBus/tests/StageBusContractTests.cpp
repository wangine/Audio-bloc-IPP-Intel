#include "StageBusValidation.h"
#include <cstring>
#include <iostream>
#include <limits>
#define CHECK(x)                                                                                   \
    do                                                                                             \
    {                                                                                              \
        if (!(x))                                                                                  \
        {                                                                                          \
            std::cerr << "FAIL line " << __LINE__ << ": " << #x << "\n";                           \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
using namespace entropy::stage;
int main()
{
    uint8_t uuid[16] = {1};
    auto s = makeSession(MIX_CINEMA10_V1, uuid, 10000000);
    CHECK(s.channels == 10 && s.sample_rate == 192000 && s.frames_per_block == 1024 &&
          s.capacity == 4);
    CHECK(validateSession(s, s.mapping_bytes) == StageBusValidationResult::Ok);
    auto eleven = makeSession(LRC_CINEMA11_V1, uuid, 10000000);
    CHECK(eleven.channels == 11);
    CHECK(eleven.slot_stride == 128 + 11 * 1024 * 8);
    auto bad = s;
    bad.channels = 11;
    CHECK(validateSession(bad, s.mapping_bytes) != StageBusValidationResult::Ok);
    bad = s;
    bad.sample_format = 2;
    CHECK(validateSession(bad, s.mapping_bytes) != StageBusValidationResult::Ok);
    bad = s;
    bad.abi_version++;
    CHECK(validateSession(bad, s.mapping_bytes) != StageBusValidationResult::Ok);
    bad = s;
    bad.slots_offset = UINT64_MAX - 63;
    CHECK(validateSession(bad, s.mapping_bytes) != StageBusValidationResult::Ok);
    bad = s;
    bad.mapping_bytes = UINT64_MAX;
    CHECK(validateSession(bad, s.mapping_bytes) != StageBusValidationResult::Ok);
    bad = s;
    bad.contract_hash[0] ^= 1;
    CHECK(validateSession(bad, s.mapping_bytes) != StageBusValidationResult::Ok);
    bad = s;
    bad.layout_hash[0] ^= 1;
    CHECK(validateSession(bad, s.mapping_bytes) != StageBusValidationResult::Ok);
    bad = s;
    bad.reserved[1] = 1;
    CHECK(validateSession(bad, s.mapping_bytes) != StageBusValidationResult::Ok);
    bad = s;
    std::memset(bad.session_id, 0, 16);
    CHECK(validateSession(bad, s.mapping_bytes) != StageBusValidationResult::Ok);
    CHECK(validateSession(s, s.mapping_bytes - 1) != StageBusValidationResult::Ok);
    StageBusBlockHeader b{};
    std::memcpy(b.session_id, uuid, 16);
    b.stream_epoch = 1;
    b.block_sequence = 1;
    b.frames = 1024;
    b.control_revision = 1;
    CHECK(validateBlock(s, b, s.slot_stride) == StageBusValidationResult::Ok);
    auto bb = b;
    bb.frames = 256;
    CHECK(validateBlock(s, bb, s.slot_stride) != StageBusValidationResult::Ok);
    bb = b;
    bb.session_id[0] = 3;
    CHECK(validateBlock(s, bb, s.slot_stride) != StageBusValidationResult::Ok);
    bb = b;
    bb.block_sequence = 0;
    CHECK(validateBlock(s, bb, s.slot_stride) != StageBusValidationResult::Ok);
    bb = b;
    bb.first_frame = 1;
    CHECK(validateBlock(s, bb, s.slot_stride) != StageBusValidationResult::Ok);
    bb = b;
    bb.block_sequence = UINT64_MAX;
    CHECK(validateBlock(s, bb, s.slot_stride) != StageBusValidationResult::Ok);
    bb = b;
    bb.validity_mask = 8;
    CHECK(validateBlock(s, bb, s.slot_stride) != StageBusValidationResult::Ok);
    StageBusFrameRequest r{};
    std::memcpy(r.session_id, uuid, 16);
    r.stream_epoch = 1;
    r.block_sequence = 1;
    r.control_revision = 1;
    r.frames = 1024;
    r.program.required_inputs = 1;
    r.program.music_program_active = 1;
    CHECK(validateRequest(s, r) == StageBusValidationResult::Ok);
    r.program.required_inputs = 0;
    CHECK(validateRequest(s, r) != StageBusValidationResult::Ok);
    r.program.required_inputs = 1;
    r.program.hardware_armed = 2;
    CHECK(validateRequest(s, r) != StageBusValidationResult::Ok);
    std::cout << "PASS StageBusContract layout 10/11, bounds, hashes, identities, sequence and "
                 "controls\n";
}
