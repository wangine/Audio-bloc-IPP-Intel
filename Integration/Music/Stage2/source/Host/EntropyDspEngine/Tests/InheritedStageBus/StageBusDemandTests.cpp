#include "StageBusDemand.h"
#include <cstring>
#include <iostream>
#define CHECK(x)                                                                                   \
    do                                                                                             \
    {                                                                                              \
        if (!(x))                                                                                  \
        {                                                                                          \
            std::cerr << "FAIL " << __LINE__ << ": " << #x << "\n";                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
using namespace entropy::stage;
int main()
{
    uint8_t uuid[16] = {8};
    auto s = makeSession(1, uuid, 10000000);
    StageBusDemand d(s);
    StageBusFrameRequest r{};
    std::memcpy(r.session_id, uuid, 16);
    r.stream_epoch = 1;
    r.block_sequence = 1;
    r.control_revision = 1;
    r.frames = 1024;
    r.program.required_inputs = 1;
    CHECK(d.accept(r) == DemandResult::Accepted);
    CHECK(d.pending());
    CHECK(d.accept(r) == DemandResult::Duplicate);
    auto changed = r;
    changed.program.manual_mute_mask = 1;
    CHECK(d.accept(changed) == DemandResult::Fault);
    auto next = r;
    next.block_sequence = 2;
    next.first_frame = 1024;
    CHECK(d.accept(next) == DemandResult::Fault);
    CHECK(!d.complete(2));
    CHECK(d.complete(1));
    CHECK(!d.pending());
    CHECK(d.accept(r) == DemandResult::Duplicate);
    CHECK(d.accept(next) == DemandResult::Accepted);
    CHECK(d.complete(2));
    next.block_sequence = 4;
    next.first_frame = 3072;
    CHECK(d.accept(next) == DemandResult::Fault);
    next.block_sequence = 3;
    next.first_frame = 2048;
    next.stream_epoch = 2;
    CHECK(d.accept(next) == DemandResult::Fault);
    std::cout << "PASS one outstanding request, duplicate idempotence, snapshot conflict, gap and "
                 "epoch\n";
}
