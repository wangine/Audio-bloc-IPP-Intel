#include "MusicTestSupport.h"
#include "DspStageRuntime.h"
#include <entropy/dynamic_recovery_abi.h>
#include <cmath>

int main() {
    using namespace entropy::publish;
    std::string error;
    auto endpoint = musicEndpoint(L".recovery");
    auto sessionHeader = session(3);
    sessionHeader.producer_pid = GetCurrentProcessId();
    Publisher source, output;
    CHECK(source.start(endpoint, error));
    source.setSession(sessionHeader);
    CHECK(output.start(endpoint + L".out", error));
    DspStageRuntime runtime;
    runtime.setPublisher(&output);
    EntropyDynamicRecoveryConfigV1 config{};
    config.struct_size = sizeof(config);
    config.version = ENTROPY_DYNAMIC_RECOVERY_SCHEMA_VERSION;
    config.transient_strength = 0.35;
    config.onset_threshold_db = 2;
    config.max_attack_boost_db = 2;
    config.expansion_ratio = 1.15;
    config.expansion_threshold_dbfs = -18;
    config.max_expansion_boost_db = 1.5;
    config.max_total_boost_db = 3;
    config.wet_mix = 1;
    CHECK(runtime.prepareRecovery(CENTER_MODULE_ROOT, config, error));
    if (!musicConnect(source, [&] { return runtime.attachPassive(endpoint, error); })) {
        std::cerr << "attachPassive: " << error << "\n";
        return 1;
    }
    Subscriber subscriber;
    CHECK(musicConnect(output, [&] { return subscriber.connect(endpoint + L".out", error); }));
    Planes<2> in;
    Planes<3> out;
    auto exchange = [&](uint64_t sequence) -> bool {
        source.service();
        auto header = musicHeader(sessionHeader, sequence);
        source.publish(header, in.in.data());
        auto processed = runtime.step(qpc());
        StageBusBlockHeader received{};
        auto read = subscriber.read(received, out.out.data());
        return processed == DspStepResult::Published && read == ReadResult::Block &&
            !std::memcmp(&header, &received, sizeof(header));
    };
    for (uint64_t sequence = 1; sequence <= 20; ++sequence) {
        in.data[0].fill(0.04);
        in.data[1].fill(0.02);
        CHECK(exchange(sequence));
        CHECK(!std::memcmp(in.in[0], out.in[0], 8192));
        CHECK(!std::memcmp(in.in[1], out.in[1], 8192));
        for (double center : out.data[2]) CHECK(center == 0.0);
    }
    config.enabled = 1;
    CHECK(runtime.applyRecovery(config));
    for (uint64_t sequence = 21; sequence <= 40; ++sequence) {
        in.data[0].fill(0.04);
        in.data[1].fill(0.02);
        CHECK(exchange(sequence));
    }
    in.data[0].fill(0.04);
    in.data[1].fill(0.02);
    for (int i = 0; i < 64; ++i) {
        in.data[0][i] = 0.8;
        in.data[1][i] = 0.4;
    }
    CHECK(exchange(41));
    const double gainL = out.data[0][32] / in.data[0][32];
    const double gainR = out.data[1][32] / in.data[1][32];
    CHECK(gainL > 1.02 && std::abs(gainL - gainR) < 1e-12);
    config.enabled = 0;
    CHECK(runtime.applyRecovery(config));
    CHECK(exchange(42));
    CHECK(!std::memcmp(in.in[0], out.in[0], 8192));
    CHECK(!std::memcmp(in.in[1], out.in[1], 8192));
    subscriber.close();
    runtime.stop();
    runtime.setPublisher(nullptr);
    std::cout << "PASS recovery OFF exact, ON stereo-linked, Center branch unchanged, OFF exact again\n";
}
