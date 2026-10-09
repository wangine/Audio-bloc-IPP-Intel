#include "SourceDemandHarness.h"
#include <QCoreApplication>
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
using namespace entropy::host;
using namespace entropy::stage;
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    uint8_t uuid[16] = {1};
    auto session = makeSession(1, uuid, 10000000);
    SourceDemandHarness h;
    CHECK(h.begin(session, SourceDemandOwner::Standalone));
    CHECK(!h.begin(session, SourceDemandOwner::External));
    StageBusProgramSnapshot p{};
    p.required_inputs = 3;
    p.music_program_active = 1;
    StageBusFrameRequest r{};
    CHECK(h.next(1, r) && r.block_sequence == 1 && r.first_frame == 0);
    CHECK(!h.next(1, r) && !h.quiesce());
    CHECK(h.complete(1));
    CHECK(h.quiesce());
    CHECK(!h.begin(session, SourceDemandOwner::Standalone));
    uuid[0] = 2;
    session = makeSession(1, uuid, 10000000);
    CHECK(h.begin(session, SourceDemandOwner::External));
    CHECK(!h.next(1, r));
    CHECK(h.quiesce());
    uuid[0] = 3;
    session = makeSession(1, uuid, 10000000);
    CHECK(h.begin(session, SourceDemandOwner::Standalone));
    CHECK(h.next(2, r) && r.block_sequence == 1);
    CHECK(h.complete(1));
    std::cout << "PASS mutually-exclusive demand owners, quiescent new UUID and timeline\n";
}
