#include "Core/SourceSignalFreshness.h"
#include <cstdio>
#define CHECK(x) do { if (!(x)) { std::printf("FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main() {
    using namespace std::chrono_literals;
    entropy::host::SourceSignalFreshness signal;
    const auto start = entropy::host::SourceSignalFreshness::Clock::time_point{} + 1s;
    CHECK(!signal.fresh(start));
    signal.captured(start);
    CHECK(signal.fresh(start + 249ms));
    CHECK(!signal.fresh(start + 251ms)); // No new frames, including a blocked read.
    signal.captured(start + 1s);
    CHECK(signal.fresh(start + 1100ms));
    signal.reset();
    CHECK(!signal.fresh(start + 1100ms));
    std::puts("PASS signal expires without capture, resumes on new frames, resets on prepare");
}
