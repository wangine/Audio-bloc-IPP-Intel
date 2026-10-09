// Frozen baseline b928941; identifier renames only. Test oracle, never linked into release.
#include "BaselineSourceBlockExecutor.h"
#include <cstring>
namespace entropy::host
{
bool BaselineSourceBlockExecutor::prepare(SourceGraphRuntime &graph, std::string &error)
{
    matrix_ = graph.findInstance("matrix.input");
    api_ = {};
    api_.struct_size = sizeof(api_);
    return matrix_ &&
           matrix_->queryExtension(ENTROPY_INPUT_MATRIX_EXTENSION_ID,
                                   ENTROPY_INPUT_MATRIX_EXTENSION_VERSION, &api_, sizeof(api_),
                                   error) &&
           api_.set_state && matrix_->api().process_f64;
}
BaselineSourceExecutionResult BaselineSourceBlockExecutor::process(const StageBusFrameRequest &request,
                                                   const AudioBlockReport &music,
                                                   const AudioBlockReport &aux,
                                                   const AudioBlockReport &cinema,
                                                   double *const *output) noexcept
{
    BaselineSourceExecutionResult result{};
    if (!matrix_ || !output || request.frames != 1024)
    {
        result.fault = 1;
        return result;
    }
    for (unsigned c = 0; c < 10; ++c)
    {
        if (!output[c])
        {
            result.fault = 2;
            return result;
        }
        std::memset(output[c], 0, 8192);
    }
    EntropyInputMatrixState state{};
    state.struct_size = sizeof(state);
    state.mute_tidal = (request.program.effective_mute_mask & 1) != 0;
    state.mute_aux = (request.program.effective_mute_mask & 2) != 0;
    state.mute_cinema = (request.program.effective_mute_mask & 4) != 0;
    if (api_.set_state(matrix_->handle(), &state) != ENTROPY_OK)
    {
        result.fault = 3;
        return result;
    }
    std::array<const double *, 12> input{};
    input.fill(silence_.data());
    std::array<double *, 12> matrixOutputs{};
    for (unsigned c = 0; c < 12; ++c)
        matrixOutputs[c] = matrixOutput_[c].data();
    const AudioBlockReport *sources[] = {&music, &aux, &cinema};
    const unsigned starts[] = {0, 2, 4}, counts[] = {2, 2, 8};
    for (unsigned role = 0; role < 3; ++role)
    {
        auto &source = *sources[role];
        bool available = source.passed;
        result.header.source_generation[role] = source.streamGeneration;
        result.header.source_state[role] = source.state;
        result.header.source_rate[role] = source.sourceRate;
        if (available)
        {
            if (source.convertedAudioPointerFrames < 1024)
            {
                result.fault = 4;
                return result;
            }
            for (unsigned c = 0; c < counts[role]; ++c)
            {
                if (!source.convertedAudioPointers[c])
                {
                    result.fault = 4;
                    return result;
                }
                input[starts[role] + c] = source.convertedAudioPointers[c];
            }
            result.header.validity_mask |= 1u << role;
        }
    }
    EntropyAudioBlockF64 b{};
    b.struct_size = sizeof(b);
    b.input_channels = b.output_channels = 12;
    b.frames = 1024;
    b.inputs = input.data();
    b.outputs = matrixOutputs.data();
    if (matrix_->api().process_f64(matrix_->handle(), &b, nullptr) != ENTROPY_OK)
    {
        result.fault = 5;
        return result;
    }
    if (!ProgramMixer::process(matrixOutputs[0], matrixOutputs[1], matrixOutputs[2],
                               matrixOutputs[3], output[0], output[1], 1024, result.mix))
    {
        result.fault = 6;
        return result;
    }
    for (unsigned c = 2; c < 10; ++c)
        std::memcpy(output[c], matrixOutputs[c + 2], 8192);
    std::memcpy(result.header.session_id, request.session_id, 16);
    result.header.stream_epoch = request.stream_epoch;
    result.header.block_sequence = request.block_sequence;
    result.header.first_frame = request.first_frame;
    result.header.frames = 1024;
    result.header.control_revision = request.control_revision;
    result.passed = true;
    return result;
}
} // namespace entropy::host
