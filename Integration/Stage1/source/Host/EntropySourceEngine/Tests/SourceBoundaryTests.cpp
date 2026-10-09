#include "SourceBlockExecutor.h"
#include "reference/BaselineSourceBlockExecutor.h"
#include <cstring>
#include <iostream>
#define CHECK(x)                                                                                   \
    do                                                                                             \
    {                                                                                              \
        if (!(x))                                                                                  \
        {                                                                                          \
            std::cerr << "FAIL " << __LINE__ << ": " << #x << " detail=" << error << "\n";         \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
using namespace entropy::host;
int sourceBoundaryProof()
{
    std::string error;
    SourceGraphRuntime source, reference, frozenGraph;
    CHECK(frozenGraph.prepare(TEST_MODULE_ROOT,error));
    BaselineSourceBlockExecutor frozen;CHECK(frozen.prepare(frozenGraph,error));
    std::array<std::array<double,1024>,10> frozenOutput{};
    std::array<double*,10> frozenPointers{};
    for(unsigned i=0;i<10;++i)frozenPointers[i]=frozenOutput[i].data();
    CHECK(source.prepare(TEST_MODULE_ROOT, error));
    CHECK(reference.prepare(TEST_MODULE_ROOT, error));
    CHECK(source.instanceCount() == 5);
    CHECK(!source.findInstance("crossover.main"));
    SourceBlockExecutor executor;
    CHECK(executor.prepare(source, error));
    std::array<std::array<double, 1024>, 12> input{}, referenceMatrix{};
    std::array<std::array<double, 1024>, 10> output{}, expected{};
    std::array<const double *, 12> ip{};
    std::array<double *, 12> mp{};
    std::array<double *, 10> op{};
    for (unsigned c = 0; c < 12; ++c)
    {
        ip[c] = input[c].data();
        mp[c] = referenceMatrix[c].data();
        for (unsigned i = 0; i < 1024; ++i)
            input[c][i] = (c + 1) * 0.25 + i / 4096.0;
    }
    for (unsigned c = 0; c < 10; ++c)
        op[c] = output[c].data();
    AudioBlockReport music, aux, cinema;
    music.passed = aux.passed = cinema.passed = true;
    music.convertedAudioPointerFrames = aux.convertedAudioPointerFrames =
        cinema.convertedAudioPointerFrames = 1024;
    music.streamGeneration = 5;
    aux.streamGeneration = 6;
    cinema.streamGeneration = 7;
    music.convertedAudioPointers[0] = ip[0];
    music.convertedAudioPointers[1] = ip[1];
    aux.convertedAudioPointers[0] = ip[2];
    aux.convertedAudioPointers[1] = ip[3];
    for (unsigned c = 0; c < 8; ++c)
        cinema.convertedAudioPointers[c] = ip[c + 4];
    StageBusFrameRequest request{};
    request.session_id[0] = 1;
    request.stream_epoch = 1;
    request.control_revision = 1;
    request.frames = 1024;
    request.program.required_inputs = 3;
    request.program.music_program_active = 1;
    auto *matrix = reference.findInstance("matrix.input");
    EntropyInputMatrixApiV1 api{};
    api.struct_size = sizeof(api);
    CHECK(matrix->queryExtension(ENTROPY_INPUT_MATRIX_EXTENSION_ID,
                                 ENTROPY_INPUT_MATRIX_EXTENSION_VERSION, &api, sizeof(api), error));
    for (uint64_t seq = 1; seq <= 40; ++seq)
    {
        request.block_sequence = seq;
        request.first_frame = (seq - 1) * 1024;
        const uint32_t localMute = static_cast<uint32_t>((seq - 1) / 5);
        request.program.effective_mute_mask = (~localMute) & 7;
        request.program.music_program_active = seq < 32;
        request.program.required_inputs = seq < 32 ? 1 : 2;
        auto result = executor.process(entropy::stage::SourceFrameEnvelope(request), localMute, music, aux, cinema, op.data());
        CHECK(result.passed);
        auto baselineRequest=request;baselineRequest.program.effective_mute_mask=localMute;
        CHECK(frozen.process(baselineRequest,music,aux,cinema,frozenPointers.data()).passed);
        for(unsigned c=0;c<10;++c)CHECK(!std::memcmp(output[c].data(),frozenOutput[c].data(),8192));
        CHECK(result.header.validity_mask == 7);
        CHECK(result.header.source_generation[0] == 5 && result.header.source_generation[2] == 7);
        EntropyInputMatrixState state{};
        state.struct_size = sizeof(state);
        state.mute_tidal = (localMute & 1) != 0;
        state.mute_aux = (localMute & 2) != 0;
        state.mute_cinema = (localMute & 4) != 0;
        CHECK(api.set_state(matrix->handle(), &state) == ENTROPY_OK);
        EntropyAudioBlockF64 b{};
        b.struct_size = sizeof(b);
        b.input_channels = b.output_channels = 12;
        b.frames = 1024;
        b.inputs = ip.data();
        b.outputs = mp.data();
        CHECK(matrix->api().process_f64(matrix->handle(), &b, nullptr) == ENTROPY_OK);
        ProgramMixTelemetry mix{};
        CHECK(ProgramMixer::process(mp[0], mp[1], mp[2], mp[3], expected[0].data(),
                                        expected[1].data(), 1024, mix));
        for (unsigned c = 2; c < 10; ++c)
            std::memcpy(expected[c].data(), mp[c + 2], 8192);
        for (unsigned c = 0; c < 10; ++c)
            CHECK(std::memcmp(output[c].data(), expected[c].data(), 8192) == 0);
        if (seq == 1) CHECK(result.mix.overloadMask == 3);
    }
    music.passed = aux.passed = cinema.passed = false;
    request.block_sequence = 41;
    request.first_frame = 40 * 1024;
    CHECK(executor.process(entropy::stage::SourceFrameEnvelope(request), request.program.effective_mute_mask, music, aux, cinema, op.data()).passed);
    for (auto &channel : output)
        for (double v : channel)
            CHECK(v == 0.0);
    std::cout << "PASS SourceBoundary 40 blocks/409600 FP64 values bitwise, distinct CINEMA, "
                 "overload, native mute ramps, inactive/stale scratch\n";
return 0;
}

#ifndef ENTROPY_SOURCE_INDEPENDENCE_AGGREGATE
int main(){return sourceBoundaryProof();}
#endif
