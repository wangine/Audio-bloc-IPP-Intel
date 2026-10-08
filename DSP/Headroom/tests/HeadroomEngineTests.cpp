#include "HeadroomEngine.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
void require(bool value, const char* message)
{
    if (!value)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

struct Block
{
    std::array<std::vector<double>, 16> input, output;
    std::array<const double*, 16> inputPointers{};
    std::array<double*, 16> outputPointers{};
    EntropyAudioBlockF64 block{};

    explicit Block(unsigned frames)
    {
        for (unsigned channel = 0; channel < 16; ++channel)
        {
            input[channel].resize(frames);
            output[channel].resize(frames);
            inputPointers[channel] = input[channel].data();
            outputPointers[channel] = output[channel].data();
        }
        block.struct_size = sizeof(block);
        block.input_channels = 16;
        block.output_channels = 16;
        block.frames = frames;
        block.inputs = inputPointers.data();
        block.outputs = outputPointers.data();
    }
};

EntropyHeadroomState makeState(unsigned revision, double positive,
                               double inputHeadroom, double reserve,
                               double transitionMs)
{
    EntropyHeadroomState state{};
    state.struct_size = sizeof(state);
    state.revision = revision;
    state.group_count = 2;
    state.active_channel_mask = 0xffffu;
    state.transition_ms = transitionMs;
    for (unsigned channel = 0; channel < 16; ++channel)
        state.channel_group[channel] = channel < 8 ? 0u : 1u;
    for (unsigned group = 0; group < 2; ++group)
    {
        state.groups[group].struct_size = sizeof(state.groups[group]);
        state.groups[group].bounded = 1;
        state.groups[group].declared_positive_contribution_db = positive;
        state.groups[group].guaranteed_input_headroom_db = inputHeadroom;
        state.groups[group].preventive_reserve_db = reserve;
    }
    return state;
}
}

int main()
{
    using entropy::headroom::HeadroomEngine;
    HeadroomEngine engine;
    require(engine.prepare(48000, 512), "prepare");

    auto unity = makeState(1, 0.0, 0.0, 0.0, 5.0);
    require(engine.setState(unity) == ENTROPY_OK, "unity state");
    require(engine.telemetry().fault_flags == 0,
            "valid initial state left stale startup fault");
    Block block(256);
    for (unsigned channel = 0; channel < 16; ++channel)
        for (unsigned frame = 0; frame < 256; ++frame)
            block.input[channel][frame] = std::bit_cast<double>(
                0x3fd0000000000000ULL + channel * 4096 + frame);
    require(engine.process(block.block) == ENTROPY_OK, "unity process");
    for (unsigned channel = 0; channel < 16; ++channel)
        require(std::memcmp(block.input[channel].data(), block.output[channel].data(),
                            256 * sizeof(double)) == 0, "unity not bit exact");

    auto reduced = makeState(2, 6.020599913279624, 0.0, 0.0, 5.0);
    require(engine.setState(reduced) == ENTROPY_OK, "reduced state");
    for (auto& channel : block.input) std::fill(channel.begin(), channel.end(), 1.0);
    require(engine.process(block.block) == ENTROPY_OK, "reduction transition");
    auto telemetry = engine.telemetry();
    require(std::abs(telemetry.groups[0].current_gain - 0.5) < 1.0e-12,
            "reduced target");
    for (unsigned channel = 1; channel < 16; ++channel)
        require(block.output[channel] == block.output[0], "coherent group mismatch");

    require(engine.setMode(ENTROPY_HEADROOM_MODE_BIT_PERFECT) == ENTROPY_OK,
            "request bit-perfect mode");
    require(engine.process(block.block) == ENTROPY_OK, "bypass transition");
    require(block.output[0].front() > 0.5 && block.output[0].front() < 1.0,
            "bypass changed gain instantaneously");
    require(block.output[0].back() == 1.0, "bypass failed to reach unity");
    auto mode = engine.modeState();
    require(mode.requested_mode == ENTROPY_HEADROOM_MODE_BIT_PERFECT &&
            mode.effective_mode == ENTROPY_HEADROOM_MODE_BIT_PERFECT &&
            mode.transition_active == 0 && mode.transition_progress == 1.0,
            "bypass mode telemetry");

    for (unsigned channel = 0; channel < 16; ++channel)
        for (unsigned frame = 0; frame < 256; ++frame)
            block.input[channel][frame] = std::bit_cast<double>(
                0x3fc0000000000000ULL + channel * 8192 + frame);
    require(engine.process(block.block) == ENTROPY_OK, "steady bypass process");
    for (unsigned channel = 0; channel < 16; ++channel)
        require(std::memcmp(block.input[channel].data(), block.output[channel].data(),
                            256 * sizeof(double)) == 0,
                "steady unity bypass not bit exact");

    for (auto& channel : block.input) std::fill(channel.begin(), channel.end(), 1.0);
    require(engine.setMode(ENTROPY_HEADROOM_MODE_AUTO) == ENTROPY_OK,
            "request auto mode");
    require(engine.process(block.block) == ENTROPY_OK, "active transition");
    require(block.output[0].front() < 1.0 && block.output[0].front() > 0.5,
            "active changed gain instantaneously");
    require(std::abs(block.output[0].back() - 0.5) < 1.0e-12,
            "active failed to restore ledger gain");

    require(engine.setPolicy(ENTROPY_HEADROOM_MODE_FIXED_SAFETY_ATTENUATION,
                             6.0) == ENTROPY_OK,
            "request fixed safety attenuation");
    require(engine.process(block.block) == ENTROPY_OK, "fixed transition");
    const auto fixedTarget = std::pow(10.0, -6.0 / 20.0);
    require(std::abs(block.output[0].back() - fixedTarget) < 1.0e-12,
            "fixed safety target");
    auto policy = engine.policyState();
    require(policy.requested_mode == ENTROPY_HEADROOM_MODE_FIXED_SAFETY_ATTENUATION &&
            policy.effective_mode == ENTROPY_HEADROOM_MODE_FIXED_SAFETY_ATTENUATION &&
            policy.transition_active == 0 &&
            std::abs(policy.fixed_attenuation_db - 6.0) < 1.0e-12 &&
            std::abs(policy.fixed_target_gain - fixedTarget) < 1.0e-12,
            "fixed policy telemetry");
    require(engine.setPolicy(ENTROPY_HEADROOM_MODE_FIXED_SAFETY_ATTENUATION,
                             12.0) == ENTROPY_OK,
            "update fixed safety attenuation");
    require(engine.process(block.block) == ENTROPY_OK,
            "same-mode fixed amount transition");
    require(std::abs(block.output[0].back() - std::pow(10.0, -12.0 / 20.0)) <
            1.0e-12, "same-mode fixed amount ignored");
    require(engine.setPolicy(ENTROPY_HEADROOM_MODE_FIXED_SAFETY_ATTENUATION,
                             -0.1) == ENTROPY_ERROR_INVALID_ARGUMENT,
            "negative fixed attenuation accepted");
    require(engine.setManualGainDb(-4.0) == ENTROPY_OK &&
            engine.setMode(ENTROPY_HEADROOM_MODE_MANUAL_SAFE) == ENTROPY_OK,
            "manual safe control");
    require(engine.process(block.block) == ENTROPY_OK, "manual safe transition");
    require(std::abs(block.output[0].back() - std::pow(10.0, -4.0 / 20.0)) < 1.0e-12,
            "manual safe gain incorrect");
    const auto control = engine.controlState();
    require(control.requested_mode == ENTROPY_HEADROOM_MODE_MANUAL_SAFE &&
            std::abs(control.manual_gain_db + 4.0) < 1.0e-12,
            "manual safe control telemetry");
    require(engine.setManualGainDb(0.1) == ENTROPY_ERROR_INVALID_ARGUMENT,
            "positive manual gain accepted");

    HeadroomEngine block32, block257;
    require(block32.prepare(48000, 257) && block257.prepare(48000, 257),
            "invariance prepare");
    auto invariant = makeState(1, 6.020599913279624, 0.0, 0.0, 20.0);
    require(block32.setState(invariant) == ENTROPY_OK &&
            block257.setState(invariant) == ENTROPY_OK &&
            block32.clearFault() == ENTROPY_OK &&
            block257.clearFault() == ENTROPY_OK, "invariance state");
    std::vector<double> curve32, curve257;
    const auto render = [](HeadroomEngine& candidate, unsigned chunk,
                           std::vector<double>& curve) {
        unsigned done = 0;
        while (done < 1200)
        {
            const auto frames = (std::min)(chunk, 1200u - done);
            Block part(frames);
            for (auto& channel : part.input)
                std::fill(channel.begin(), channel.end(), 1.0);
            require(candidate.process(part.block) == ENTROPY_OK, "invariance process");
            curve.insert(curve.end(), part.output[0].begin(), part.output[0].end());
            done += frames;
        }
    };
    render(block32, 32, curve32);
    render(block257, 257, curve257);
    require(curve32 == curve257, "transition depends on callback size");

    HeadroomEngine invalid;
    require(invalid.prepare(48000, 256), "invalid prepare");
    auto unknown = makeState(1, 0.0, 0.0, 0.0, 10.0);
    unknown.groups[0].bounded = 0;
    unknown.groups[0].guaranteed_input_headroom_db =
        std::numeric_limits<double>::quiet_NaN();
    require(invalid.setState(unknown) == ENTROPY_ERROR_INVALID_ARGUMENT,
            "unknown headroom accepted");
    require((invalid.telemetry().fault_flags & ENTROPY_HEADROOM_FAULT_INVALID_LEDGER) != 0,
            "invalid ledger not latched");


    // Regression: AUTO is a single global 16-channel gain derived from the
    // pre-gain terminal peak. A sustained +6.0206 dBFS sample on CH8 must
    // converge once to 0.5 and must not compound the reduction every block.
    HeadroomEngine autoGlobal;
    require(autoGlobal.prepare(8000, 256), "auto global prepare");
    require(autoGlobal.setManualGainDb(-12.0)==ENTROPY_OK,"auto test range");
    auto autoState = makeState(1, 0.0, 0.0, 0.0, 50.0);
    autoState.group_count = 1;
    autoState.active_channel_mask = 0xffffu;
    for (auto& group : autoState.channel_group) group = 0;
    autoState.groups[0].preventive_reserve_db = 0.0;
    require(autoGlobal.setState(autoState) == ENTROPY_OK &&
            autoGlobal.clearFault() == ENTROPY_OK &&
            autoGlobal.setMode(ENTROPY_HEADROOM_MODE_AUTO) == ENTROPY_OK,
            "auto global state");
    Block autoBlock(256);
    for (unsigned channel = 0; channel < 16; ++channel)
        std::fill(autoBlock.input[channel].begin(), autoBlock.input[channel].end(), 0.25);
    std::fill(autoBlock.input[7].begin(), autoBlock.input[7].end(), 2.0);
    EntropyHeadroomAnalysisBlockF64 autoAnalysis{};
    autoAnalysis.struct_size = sizeof(autoAnalysis);
    autoAnalysis.channels = 16;
    autoAnalysis.frames = 256;
    autoAnalysis.inputs = autoBlock.inputPointers.data();
    for (int iteration = 0; iteration < 200; ++iteration)
    {
        require(autoGlobal.analyzeFinal(autoAnalysis) == ENTROPY_OK,
                "auto global analyze");
        require(autoGlobal.process(autoBlock.block) == ENTROPY_OK,
                "auto global process");
    }
    const auto autoTelemetry = autoGlobal.telemetry();
    require(std::abs(autoTelemetry.groups[0].current_gain - 0.5) < 1.0e-12,
            "AUTO global gain did not converge to 0.5");
    require(std::abs(autoTelemetry.channel_input_peak[7] - 2.0) < 1.0e-15 &&
            autoTelemetry.limiting_channel_mask == (1u << 7),
            "AUTO telemetry did not identify CH8 as limiting channel");
    for (unsigned channel = 0; channel < 16; ++channel)
        require(std::abs(autoBlock.output[channel][0] -
                    autoBlock.input[channel][0] * 0.5) < 1.0e-12,
                "AUTO did not apply one common gain to all 16 channels");
    for (int iteration = 0; iteration < 200; ++iteration)
    {
        require(autoGlobal.analyzeFinal(autoAnalysis) == ENTROPY_OK,
                "auto global steady analyze");
        require(autoGlobal.process(autoBlock.block) == ENTROPY_OK,
                "auto global steady process");
    }
    require(std::abs(autoGlobal.telemetry().groups[0].current_gain - 0.5) < 1.0e-12,
            "AUTO compounded gain reduction across blocks");

    // V8 terminal AUTO: measured quiet audio must recover independently of
    // the upstream theoretical ledger. Legacy process-only hosts retain it.
    autoState.revision = 2;
    autoState.groups[0].declared_positive_contribution_db = 9.0;
    require(autoGlobal.setState(autoState) == ENTROPY_OK, "9 dB ledger");
    for (auto& channel : autoBlock.input)
        std::fill(channel.begin(), channel.end(), 0.125);
    for (int iteration=0; iteration<300; ++iteration) {
        require(autoGlobal.analyzeFinal(autoAnalysis)==ENTROPY_OK, "quiet analysis");
        require(autoGlobal.process(autoBlock.block)==ENTROPY_OK, "quiet process");
    }
    require(autoGlobal.telemetry().groups[0].current_gain==1.0,
            "terminal AUTO stuck at theoretical ledger attenuation");
    autoState.revision = 3;
    autoState.groups[0].declared_positive_contribution_db = 0.0;
    require(autoGlobal.setState(autoState)==ENTROPY_OK, "clear ledger requirement");
    for (int iteration=0; iteration<300; ++iteration) {
        require(autoGlobal.analyzeFinal(autoAnalysis)==ENTROPY_OK, "recovery analysis");
        require(autoGlobal.process(autoBlock.block)==ENTROPY_OK, "recovery process");
    }
    require(autoGlobal.telemetry().groups[0].current_gain==1.0,
            "AUTO must recover when graph no longer requires attenuation");

    for (const double limit : {-3.5, -6.0, 0.0}) {
        require(autoGlobal.setManualGainDb(limit)==ENTROPY_OK,"set AUTO attenuation bound");
        for (auto& channel : autoBlock.input) std::fill(channel.begin(),channel.end(),8.0);
        for (int i=0;i<200;++i) {
            require(autoGlobal.analyzeFinal(autoAnalysis)==ENTROPY_OK,"bounded analysis");
            require(autoGlobal.process(autoBlock.block)==ENTROPY_OK,"bounded process");
        }
        const double floor=std::pow(10.0,limit/20.0);
        const auto bounded=autoGlobal.telemetry();
        require(std::abs(bounded.groups[0].current_gain-floor)<1e-12,
                "AUTO must respect MANUAL SAFE attenuation limit");
        require(std::abs(bounded.groups[0].target_gain-floor)<1e-12,"bounded target telemetry");
        require(std::abs(autoBlock.output[0][255]-8.0*floor)<1e-12,"bounded AUTO must not clip");
        require(bounded.analyzer_state==ENTROPY_HEADROOM_ANALYZER_CAPACITY_EXCEEDED,
                "insufficient AUTO range was not reported");
    }

    require(autoGlobal.setMode(ENTROPY_HEADROOM_MODE_BIT_PERFECT)==ENTROPY_OK,
            "over-unity passthrough mode");
    for (auto& channel : autoBlock.input) std::fill(channel.begin(),channel.end(),4.0);
    for (int i=0;i<4;++i) require(autoGlobal.process(autoBlock.block)==ENTROPY_OK,"passthrough settle");
    require(autoBlock.output[0][255]==4.0,"passthrough clipped FP64 above unity");
    require(autoGlobal.setManualGainDb(-6.020599913279624)==ENTROPY_OK &&
            autoGlobal.setMode(ENTROPY_HEADROOM_MODE_MANUAL_SAFE)==ENTROPY_OK,"manual scalar");
    for (int i=0;i<4;++i) require(autoGlobal.process(autoBlock.block)==ENTROPY_OK,"manual settle");
    require(std::abs(autoBlock.output[0][255]-2.0)<1e-12,"manual clipped FP64 above unity");

    block.input[3][17] = std::numeric_limits<double>::quiet_NaN();
    require(engine.process(block.block) == ENTROPY_OK, "nan process");
    for (const auto& channel : block.output)
        for (const auto value : channel) require(value == 0.0, "fault gate not zero");
    telemetry = engine.telemetry();
    require((telemetry.fault_flags & ENTROPY_HEADROOM_FAULT_NONFINITE_AUDIO) &&
            telemetry.nonfinite_sample_count == 1, "fault telemetry");
    block.input[3][17] = 1.0;
    require(engine.clearFault() == ENTROPY_OK &&
            engine.process(block.block) == ENTROPY_OK, "controlled recovery");

    // V2 timing semantics: sample safety is immediate because analyzeFinal()
    // sees the exact block before process(). ATTACK remains meaningful only for
    // additional preventive reserve below unity; RELEASE remains slow.
    for(const unsigned rate : {8000u,48000u,192000u}) {
        HeadroomEngine timed;
        require(timed.prepare(rate,1),"timing rate prepare");
        auto state=makeState(1,0,0,3.0,1);
        require(timed.setState(state)==ENTROPY_OK &&
                timed.telemetry().fault_flags==0 &&
                timed.setManualGainDb(-18)==ENTROPY_OK,"timing setup");
        EntropyHeadroomTimingStateV5 settings{sizeof(settings),0,200,1000};
        require(timed.setTiming(settings)==ENTROPY_OK,"timing valid");
        Block single(1); for(auto& channel:single.input)channel[0]=0.8;
        EntropyHeadroomAnalysisBlockF64 analysis{};analysis.struct_size=sizeof(analysis);
        analysis.channels=16;analysis.frames=1;analysis.inputs=single.inputPointers.data();

        const double peakDb=20.0*std::log10(0.8);
        const double preventiveTarget=std::pow(10.0,-std::max(0.0,peakDb+3.0)/20.0);
        for(unsigned n=0;n<rate/5;++n) {
            if(n==rate/20){settings.attack_ms=100;require(timed.setTiming(settings)==ENTROPY_OK,"mid-ramp timing update");}
            require(timed.analyzeFinal(analysis)==ENTROPY_OK && timed.process(single.block)==ENTROPY_OK,"timing sample");
            if(n+1==rate/10)
                require(single.output[0][0] > 0.8*preventiveTarget &&
                        single.output[0][0] < 0.8,
                        "preventive attack did not remain smooth");
        }
        require(std::abs(single.output[0][0]-0.8*preventiveTarget)<1e-10,
                "preventive attack duration");

        // A real over-unity block must be safe on its first processed sample;
        // configured attack must never postpone required safety attenuation.
        for(auto& channel:single.input)channel[0]=2.0;
        require(timed.analyzeFinal(analysis)==ENTROPY_OK &&
                timed.process(single.block)==ENTROPY_OK,"immediate safety sample");
        require(single.output[0][0] <= 1.0 + 1e-12,
                "AUTO attack allowed over-unity sample through");

        // Quiet input: one-second hold followed by configured one-second release.
        for(auto& channel:single.input)channel[0]=0.125;
        for(unsigned n=0;n<rate*2+2;++n){
            require(timed.analyzeFinal(analysis)==ENTROPY_OK &&
                    timed.process(single.block)==ENTROPY_OK,"release sample");
        }
        require(std::abs(single.output[0][0]-0.125)<1e-12,
                "hold plus configured release reaches unity");

        settings.attack_ms=std::numeric_limits<double>::quiet_NaN();
        require(timed.setTiming(settings)==ENTROPY_ERROR_INVALID_ARGUMENT &&
                timed.timing().attack_ms==100,
                "invalid timing preserves state");
    }
    std::cout << "PASS Headroom AUTO/MANUAL_SAFE/BIT_PERFECT transitions, "
                 "bit-exact copy, deterministic ledger/analyzer and safe faults\n";
    return 0;
}
