#include "PluginProcessor.h"
#include "IppSpectrum.h"

#include <entropy/crossover_abi.h>
#include <entropy/crossover_program_abi.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <new>

namespace
{
using Engine = entropy::standalone::EntropyCrossoverAudioProcessor;

constexpr uint32_t inputCount = 13;
constexpr uint32_t outputCount = 16;

struct InputDefinition
{
    uint32_t semantic;
    uint32_t program;
    uint32_t programChannel;
    const char* identifier;
    const char* label;
};

constexpr std::array<InputDefinition, inputCount> inputs {{
    { ENTROPY_CROSSOVER_INPUT_MUSIC_L, ENTROPY_CROSSOVER_PROGRAM_MUSIC, 0, "MUSIC_L", "Music Left" },
    { ENTROPY_CROSSOVER_INPUT_MUSIC_R, ENTROPY_CROSSOVER_PROGRAM_MUSIC, 1, "MUSIC_R", "Music Right" },
    { ENTROPY_CROSSOVER_INPUT_MUSIC_C, ENTROPY_CROSSOVER_PROGRAM_MUSIC, 2, "MUSIC_C", "Music Center" },
    { ENTROPY_CROSSOVER_INPUT_CINEMA_L, ENTROPY_CROSSOVER_PROGRAM_CINEMA, 0, "CINEMA_L", "Cinema Left" },
    { ENTROPY_CROSSOVER_INPUT_CINEMA_R, ENTROPY_CROSSOVER_PROGRAM_CINEMA, 1, "CINEMA_R", "Cinema Right" },
    { ENTROPY_CROSSOVER_INPUT_CINEMA_C, ENTROPY_CROSSOVER_PROGRAM_CINEMA, 2, "CINEMA_C", "Cinema Center" },
    { ENTROPY_CROSSOVER_INPUT_CINEMA_LFE, ENTROPY_CROSSOVER_PROGRAM_CINEMA, 3, "CINEMA_LFE", "Cinema LFE" },
    { ENTROPY_CROSSOVER_INPUT_CINEMA_SL, ENTROPY_CROSSOVER_PROGRAM_CINEMA, 4, "CINEMA_SL", "Cinema Surround Left" },
    { ENTROPY_CROSSOVER_INPUT_CINEMA_SR, ENTROPY_CROSSOVER_PROGRAM_CINEMA, 5, "CINEMA_SR", "Cinema Surround Right" },
    { ENTROPY_CROSSOVER_INPUT_CINEMA_BL, ENTROPY_CROSSOVER_PROGRAM_CINEMA, 6, "CINEMA_BL", "Cinema Back Left" },
    { ENTROPY_CROSSOVER_INPUT_CINEMA_BR, ENTROPY_CROSSOVER_PROGRAM_CINEMA, 7, "CINEMA_BR", "Cinema Back Right" },
    { ENTROPY_CROSSOVER_INPUT_CINEMA_TSL, ENTROPY_CROSSOVER_PROGRAM_CINEMA, 8, "CINEMA_TSL", "Cinema Top Side Left" },
    { ENTROPY_CROSSOVER_INPUT_CINEMA_TSR, ENTROPY_CROSSOVER_PROGRAM_CINEMA, 9, "CINEMA_TSR", "Cinema Top Side Right" }
}};

struct CrossoverState
{
    std::unique_ptr<Engine> engine;
    entropy::native::AudioBuffer<double> processingBuffer;
    entropy::native::MidiBuffer midi;
    EntropyPrepareInfo prepare {};
    bool prepared = false;
    static constexpr int viewFftOrder = 14;
    static constexpr int viewFftSize = 1 << viewFftOrder;
    entropy::native::IppSpectrum viewFft { viewFftOrder };
    std::array<float, viewFftSize> viewFftData {};
    std::array<double, viewFftSize> viewWindow {}, viewFftInput {};
    std::array<double, viewFftSize / 2 + 1> viewFftMagnitude {};
    std::array<float, viewFftSize / 2 + 1> viewSpectrumSmoothed {};
    std::array<std::array<float, ENTROPY_CROSSOVER_RESPONSE_POINTS>, 16> viewResponseCache {};
    std::array<float, ENTROPY_CROSSOVER_SPECTRUM_POINTS> viewSpectrumFrequencyCache {};
    std::array<float, ENTROPY_CROSSOVER_SPECTRUM_POINTS> viewSpectrumDbCache {};
    double viewResponseLastUpdateMs = -1000.0;
    double viewSpectrumLastUpdateMs = -1000.0;
    bool viewSpectrumAvailable = false;
    int monitoredChannel = -1;
    bool controlTransactionOpen = false;
    bool controlDuplicateCandidate = false;
    Engine::ControlCommandTag controlTransaction {};
    entropy::native::MemoryBlock controlTransactionRollback;
    bool lastCommittedValid = false;
    Engine::ControlCommandTag lastCommitted {};

    CrossoverState()
    {
        viewSpectrumSmoothed.fill(-120.0f);
        double windowSum = 0.0;
        for (int i=0; i<viewFftSize; ++i) {
            viewWindow[i] = 0.5 - 0.5 * std::cos(6.283185307179586476925286766559 * i / (viewFftSize-1));
            windowSum += viewWindow[i];
        }
        for (auto& value : viewWindow) value *= viewFftSize / windowSum;
    }
};

constexpr uint64_t fnvOffset = 1469598103934665603ULL;
constexpr uint64_t fnvPrime = 1099511628211ULL;

void hashBytes(uint64_t& hash, const void* bytes, std::size_t size) noexcept
{
    const auto* cursor = static_cast<const unsigned char*>(bytes);
    for (std::size_t index = 0; index < size; ++index)
        hash = (hash ^ cursor[index]) * fnvPrime;
}

void hashParameter(Engine::ControlCommandTag& tag, const char* identifier, double value) noexcept
{
    const auto length = static_cast<uint32_t>(std::strlen(identifier));
    hashBytes(tag.payloadHash, &length, sizeof(length));
    hashBytes(tag.payloadHash, identifier, length);
    uint64_t valueBits = 0;
    std::memcpy(&valueBits, &value, sizeof(valueBits));
    hashBytes(tag.payloadHash, &valueBits, sizeof(valueBits));
}

void hashIdentifier(Engine::ControlCommandTag& tag, const char* identifier) noexcept
{
    const auto length = static_cast<uint32_t>(std::strlen(identifier));
    hashBytes(tag.payloadHash, &length, sizeof(length));
    hashBytes(tag.payloadHash, identifier, length);
}

void rollbackControlTransaction(CrossoverState& state) noexcept
{
    if (!state.controlDuplicateCandidate && state.controlTransactionRollback.getSize() != 0)
    {
        try
        {
            (void) state.engine->restoreStateInformation(
                state.controlTransactionRollback.getData(),
                static_cast<int>(state.controlTransactionRollback.getSize()));
        }
        catch (...) {}
    }
    state.controlTransactionRollback.setSize(0);
    state.controlDuplicateCandidate = false;
}

void acceptControlTransaction(CrossoverState& state) noexcept
{
    state.controlTransactionRollback.setSize(0);
    state.controlDuplicateCandidate = false;
}

void copyText(char* destination, std::size_t capacity, const char* source)
{
    if (capacity == 0) return;
    const auto* text = source != nullptr ? source : "";
    const auto length = (std::min)(capacity-1, std::strlen(text));
    std::memcpy(destination,text,length);
    destination[length] = '\0';
}

CrossoverState* stateFrom(EntropyModuleHandle handle) noexcept
{
    return reinterpret_cast<CrossoverState*> (handle);
}

EntropyResult assignParameterValue(CrossoverState& state, const char* identifier, double value)
{
    if (identifier == nullptr || ! std::isfinite(value)) return ENTROPY_ERROR_INVALID_ARGUMENT;
    auto* parameter = state.engine->getAPVTS().getParameter(identifier);
    if (parameter == nullptr) return ENTROPY_ERROR_UNSUPPORTED;
    parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float> (value)));
    return ENTROPY_OK;
}

EntropyResult setParameterValue(CrossoverState& state, const char* identifier, double value)
{
    if (state.controlTransactionOpen && state.controlDuplicateCandidate)
    {
        if (identifier == nullptr || !std::isfinite(value))
            return ENTROPY_ERROR_INVALID_ARGUMENT;
        if (state.engine->getAPVTS().getParameter(identifier) == nullptr)
            return ENTROPY_ERROR_UNSUPPORTED;
        hashParameter(state.controlTransaction, identifier, value);
        return ENTROPY_OK;
    }
    const auto result = assignParameterValue(state, identifier, value);
    if (result != ENTROPY_OK) return result;
    if (state.controlTransactionOpen)
    {
        hashParameter(state.controlTransaction, identifier, value);
        return ENTROPY_OK;
    }
    state.engine->serviceUpdates();
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL beginControlCommand(
    EntropyModuleHandle handle, const EntropyCrossoverCommandIdV1* id, uint32_t kind)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || id == nullptr || id->struct_size < sizeof(*id) ||
        id->extension_version != ENTROPY_CROSSOVER_CONTROL_STATUS_EXTENSION_VERSION ||
        id->session == 0 || id->sequence == 0 ||
        kind < ENTROPY_CROSSOVER_COMMAND_PARAMETER ||
        kind > ENTROPY_CROSSOVER_COMMAND_CONTROL_ONLY ||
        state->controlTransactionOpen)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    Engine::ControlCommandTag command{ id->session, id->sequence, kind, fnvOffset };
    hashBytes(command.payloadHash, &kind, sizeof(kind));
    if (state->lastCommittedValid && state->lastCommitted.session == id->session &&
        id->sequence < state->lastCommitted.sequence)
    {
        state->engine->publishControlFailure(
            command, ENTROPY_CROSSOVER_COMMAND_REASON_SESSION_MISMATCH);
        return ENTROPY_ERROR_INVALID_STATE;
    }
    state->controlDuplicateCandidate = state->lastCommittedValid &&
        state->lastCommitted.session == id->session &&
        state->lastCommitted.sequence == id->sequence;
    state->controlTransactionRollback.setSize(0);
    if (!state->controlDuplicateCandidate)
    {
        try { state->engine->getStateInformation(state->controlTransactionRollback); }
        catch (...) { return ENTROPY_ERROR_INVALID_STATE; }
        if (state->controlTransactionRollback.getSize() == 0)
            return ENTROPY_ERROR_INVALID_STATE;
    }
    state->controlTransactionOpen = true;
    state->controlTransaction = command;
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL commitControlCommand(EntropyModuleHandle handle)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || !state->controlTransactionOpen)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    const auto command = state->controlTransaction;
    state->controlTransactionOpen = false;
    state->controlTransaction = {};
    if (state->controlDuplicateCandidate)
    {
        if (state->lastCommitted.payloadHash == command.payloadHash &&
            state->lastCommitted.kind == command.kind)
        {
            acceptControlTransaction(*state);
            return ENTROPY_OK;
        }
        rollbackControlTransaction(*state);
        state->engine->publishControlFailure(
            command, ENTROPY_CROSSOVER_COMMAND_REASON_DUPLICATE_PAYLOAD_MISMATCH);
        return ENTROPY_ERROR_INVALID_STATE;
    }
    if (command.kind == ENTROPY_CROSSOVER_COMMAND_GESTURE_BEGIN ||
        command.kind == ENTROPY_CROSSOVER_COMMAND_GESTURE_END ||
        command.kind == ENTROPY_CROSSOVER_COMMAND_CONTROL_ONLY)
    {
        state->engine->publishControlAccepted(command);
        state->lastCommitted = command;
        state->lastCommittedValid = true;
        acceptControlTransaction(*state);
        return ENTROPY_OK;
    }
    if (!state->engine->publishControlSnapshot(command))
    {
        rollbackControlTransaction(*state);
        state->engine->publishControlFailure(
            command, ENTROPY_CROSSOVER_COMMAND_REASON_QUEUE_FULL);
        return ENTROPY_ERROR_INVALID_STATE;
    }
    state->lastCommitted = command;
    state->lastCommittedValid = true;
    acceptControlTransaction(*state);
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL cancelControlCommand(EntropyModuleHandle handle, uint32_t reason)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || !state->controlTransactionOpen ||
        reason == ENTROPY_CROSSOVER_COMMAND_REASON_NONE ||
        reason > ENTROPY_CROSSOVER_COMMAND_REASON_INTERNAL_ERROR)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    const auto command = state->controlTransaction;
    state->controlTransactionOpen = false;
    state->controlTransaction = {};
    rollbackControlTransaction(*state);
    state->engine->publishControlFailure(command, reason);
    return ENTROPY_OK;
}

void writeCommandId(EntropyCrossoverCommandIdV1& destination,
                    const Engine::ControlCommandTag& source) noexcept
{
    destination = {};
    destination.struct_size = sizeof(destination);
    destination.extension_version = ENTROPY_CROSSOVER_CONTROL_STATUS_EXTENSION_VERSION;
    destination.session = source.session;
    destination.sequence = source.sequence;
}

EntropyResult ENTROPY_ABI_CALL getControlCommandStatus(
    EntropyModuleHandle handle, EntropyCrossoverControlStatusV1* destination)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || destination == nullptr ||
        destination->struct_size < sizeof(*destination) ||
        destination->extension_version != ENTROPY_CROSSOVER_CONTROL_STATUS_EXTENSION_VERSION)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    const auto status = state->engine->controlCommandStatus();
    *destination = {};
    destination->struct_size = sizeof(*destination);
    destination->extension_version = ENTROPY_CROSSOVER_CONTROL_STATUS_EXTENSION_VERSION;
    writeCommandId(destination->accepted, status.accepted);
    writeCommandId(destination->applied, status.applied);
    writeCommandId(destination->failed, status.failed);
    destination->reason = status.reason;
    destination->kind = status.accepted.sequence != 0 ? status.accepted.kind
                                                       : status.failed.kind;
    if (status.accepted.sequence != 0 &&
        status.applied.session == status.accepted.session &&
        status.applied.sequence == status.accepted.sequence)
        destination->state = ENTROPY_CROSSOVER_COMMAND_APPLIED;
    else if (status.accepted.sequence != 0)
        destination->state = ENTROPY_CROSSOVER_COMMAND_ACCEPTED;
    else if (status.failed.sequence != 0)
        destination->state = ENTROPY_CROSSOVER_COMMAND_FAILED;
    else
        destination->state = ENTROPY_CROSSOVER_COMMAND_IDLE;
    return ENTROPY_OK;
}

EntropyResult parameterGesture(CrossoverState& state, const char* identifier, bool begin)
{
    if (identifier == nullptr) return ENTROPY_ERROR_INVALID_ARGUMENT;
    auto* parameter = state.engine->getAPVTS().getParameter(identifier);
    if (parameter == nullptr) return ENTROPY_ERROR_UNSUPPORTED;
    if (state.controlTransactionOpen)
    {
        hashIdentifier(state.controlTransaction, identifier);
        if (state.controlDuplicateCandidate) return ENTROPY_OK;
    }
    // Gestures are control-only in the native headless module. Transaction
    // identity/hashing above is preserved; no editor/listener dispatch exists.
    (void) begin;
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL beginParameterGesture(
    EntropyModuleHandle handle, const char* identifier)
{
    auto* state = stateFrom(handle);
    return state != nullptr ? parameterGesture(*state, identifier, true)
                            : ENTROPY_ERROR_INVALID_ARGUMENT;
}

EntropyResult ENTROPY_ABI_CALL endParameterGesture(
    EntropyModuleHandle handle, const char* identifier)
{
    auto* state = stateFrom(handle);
    return state != nullptr ? parameterGesture(*state, identifier, false)
                            : ENTROPY_ERROR_INVALID_ARGUMENT;
}

void outputRole(uint32_t engineRole, uint32_t& semantic, uint32_t& way) noexcept
{
    semantic = ENTROPY_CROSSOVER_OUTPUT_OFF;
    way = 0;
    if (engineRole >= 1 && engineRole <= 5) { semantic = ENTROPY_CROSSOVER_OUTPUT_LEFT; way = engineRole; }
    else if (engineRole >= 6 && engineRole <= 10) { semantic = ENTROPY_CROSSOVER_OUTPUT_RIGHT; way = engineRole - 5; }
    else if (engineRole >= 11 && engineRole <= 15) { semantic = ENTROPY_CROSSOVER_OUTPUT_CENTER; way = engineRole - 10; }
    else if (engineRole == 16 || engineRole == 18) { semantic = ENTROPY_CROSSOVER_OUTPUT_SUB; way = engineRole == 16 ? 1u : 2u; }
    else if (engineRole == 17 || engineRole == 19) { semantic = ENTROPY_CROSSOVER_OUTPUT_SCOOP; way = engineRole == 17 ? 1u : 2u; }
    else if (engineRole >= 20 && engineRole <= 25) semantic = ENTROPY_CROSSOVER_OUTPUT_CINEMA_SL + engineRole - 20;
}

EntropyResult ENTROPY_ABI_CALL getDescriptor(EntropyModuleDescriptor* descriptor)
{
    if (descriptor == nullptr || descriptor->struct_size < sizeof(*descriptor))
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    *descriptor = {};
    descriptor->struct_size = sizeof(*descriptor);
    descriptor->abi_version = ENTROPY_MODULE_ABI_VERSION;
    descriptor->module_kind = ENTROPY_MODULE_CROSSOVER;
    descriptor->module_flags = ENTROPY_MODULE_FLAG_REALTIME_SAFE_PROCESS |
        ENTROPY_MODULE_FLAG_FP64 | ENTROPY_MODULE_FLAG_REPORTS_LATENCY;
    descriptor->version_major = 3;
    descriptor->version_minor = 0;
    descriptor->version_patch = 0;
    descriptor->max_input_channels = inputCount;
    descriptor->max_output_channels = outputCount;
    copyText(descriptor->identifier, sizeof(descriptor->identifier), "audio.entropy.crossover");
    copyText(descriptor->name, sizeof(descriptor->name), "Entropy Crossover IPP");
    copyText(descriptor->vendor, sizeof(descriptor->vendor), "Entropy Audio");
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL createModule(const EntropyHostServices* host, EntropyModuleHandle* result)
{
    if (host == nullptr || result == nullptr || host->struct_size < sizeof(*host) ||
        host->abi_version != ENTROPY_MODULE_ABI_VERSION)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    try
    {
        auto state = std::make_unique<CrossoverState>();
        state->engine = std::make_unique<Engine>();
        *result = reinterpret_cast<EntropyModuleHandle> (state.release());
        return ENTROPY_OK;
    }
    catch (...) { return ENTROPY_ERROR_INVALID_STATE; }
}

void ENTROPY_ABI_CALL destroyModule(EntropyModuleHandle handle)
{
    delete stateFrom(handle);
}

EntropyResult ENTROPY_ABI_CALL prepareModule(EntropyModuleHandle handle, const EntropyPrepareInfo* info)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || info == nullptr || info->struct_size < sizeof(*info) ||
        !std::isfinite(info->sample_rate) || info->sample_rate <= 0 ||
        info->sample_rate > 768000.0 || info->maximum_frames == 0 ||
        info->maximum_frames > static_cast<uint32_t>(std::numeric_limits<int>::max() - 8192) ||
        info->input_channels != inputCount || info->output_channels != outputCount)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    try
    {
        state->processingBuffer.setSize(outputCount, static_cast<int> (info->maximum_frames),
                                        false, false, true);
        state->prepare = *info;
        state->engine->prepareToPlay(info->sample_rate, static_cast<int> (info->maximum_frames));
        state->prepared = true;
        return ENTROPY_OK;
    }
    catch (...) { state->prepared = false; return ENTROPY_ERROR_INVALID_STATE; }
}

EntropyResult ENTROPY_ABI_CALL resetModule(EntropyModuleHandle handle)
{
    auto* state = stateFrom(handle);
    if (state == nullptr) return ENTROPY_ERROR_INVALID_ARGUMENT;
    if (! state->prepared) return ENTROPY_ERROR_NOT_PREPARED;
    try
    {
        state->engine->prepareToPlay(state->prepare.sample_rate,
                                     static_cast<int> (state->prepare.maximum_frames));
        return ENTROPY_OK;
    }
    catch (...) { return ENTROPY_ERROR_INVALID_STATE; }
}

EntropyResult ENTROPY_ABI_CALL importState(EntropyModuleHandle handle, const void* bytes, uint32_t byteCount)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || bytes == nullptr || byteCount == 0) return ENTROPY_ERROR_INVALID_ARGUMENT;
    try
    {
        if (state->controlTransactionOpen && state->controlDuplicateCandidate)
        {
            hashBytes(state->controlTransaction.payloadHash, bytes, byteCount);
            return ENTROPY_OK;
        }
        if (! state->engine->restoreStateInformation(bytes, static_cast<int> (byteCount)))
            return ENTROPY_ERROR_INVALID_STATE;
        if (state->controlTransactionOpen)
        {
            hashBytes(state->controlTransaction.payloadHash, bytes, byteCount);
            return ENTROPY_OK;
        }
        if (state->prepared)
            state->engine->prepareToPlay(state->prepare.sample_rate,
                                         static_cast<int> (state->prepare.maximum_frames));
        else
            state->engine->serviceUpdates();
        return ENTROPY_OK;
    }
    catch (...) { return ENTROPY_ERROR_INVALID_STATE; }
}

EntropyResult ENTROPY_ABI_CALL setConfig(EntropyModuleHandle handle, const EntropyConfigBlob* config)
{
    if (config == nullptr || config->struct_size < sizeof(*config) ||
        config->schema_id != ENTROPY_CROSSOVER_STATE_SCHEMA ||
        config->schema_version != ENTROPY_CROSSOVER_STATE_VERSION)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    return importState(handle, config->bytes, config->byte_count);
}

EntropyResult ENTROPY_ABI_CALL processModule(EntropyModuleHandle handle,
                                             const EntropyAudioBlockF64* block,
                                             const EntropyProcessContext*)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || block == nullptr || block->struct_size < sizeof(*block))
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    if (! state->prepared) return ENTROPY_ERROR_NOT_PREPARED;
    if (block->frames > state->prepare.maximum_frames ||
        block->input_channels != inputCount || block->output_channels != outputCount ||
        block->inputs == nullptr || block->outputs == nullptr)
        return ENTROPY_ERROR_INVALID_ARGUMENT;

    if (block->frames == 0) return ENTROPY_OK;

    // Validate all pointers and input samples before advancing any engine state.
    for (uint32_t channel = 0; channel < outputCount; ++channel)
        if (block->outputs[channel] == nullptr) return ENTROPY_ERROR_INVALID_ARGUMENT;
    for (uint32_t channel = 0; channel < inputCount; ++channel) {
        if (block->inputs[channel] == nullptr) return ENTROPY_ERROR_INVALID_ARGUMENT;
        for (uint32_t frame = 0; frame < block->frames; ++frame)
            if (!std::isfinite(block->inputs[channel][frame])) return ENTROPY_ERROR_PROCESSING;
    }
    const auto frames = static_cast<int> (block->frames);
    for (uint32_t channel = 0; channel < inputCount; ++channel)
    {
        if (block->inputs[channel] == nullptr) return ENTROPY_ERROR_INVALID_ARGUMENT;
        state->processingBuffer.copyFrom(static_cast<int> (channel), 0,
                                         block->inputs[channel], frames);
    }
    for (uint32_t channel = inputCount; channel < outputCount; ++channel)
        state->processingBuffer.clear(static_cast<int> (channel), 0, frames);

    try
    {
        // Prepared native storage and an inline non-owning view advance only frames.
        std::array<double*,outputCount> preparedPointers{};
        for (uint32_t c=0; c<outputCount; ++c)
            preparedPointers[c] = state->processingBuffer.getWritePointer(static_cast<int>(c));
        entropy::native::AudioBuffer<double> blockView(preparedPointers.data(),
                                             static_cast<int> (outputCount), frames);
        state->engine->processBlock(blockView, state->midi);
    }
    catch (...) { return ENTROPY_ERROR_PROCESSING; }

    for (uint32_t channel = 0; channel < outputCount; ++channel)
    {
        if (block->outputs[channel] == nullptr) return ENTROPY_ERROR_INVALID_ARGUMENT;
        const auto* source = state->processingBuffer.getReadPointer(static_cast<int> (channel));
        for (uint32_t frame = 0; frame < block->frames; ++frame)
            if (! std::isfinite(source[frame])) return ENTROPY_ERROR_PROCESSING;
    }
    for (uint32_t channel = 0; channel < outputCount; ++channel)
        std::copy_n(state->processingBuffer.getReadPointer(static_cast<int>(channel)),
                    block->frames, block->outputs[channel]);
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL requiredProgramInputs(EntropyModuleHandle handle, uint32_t* inputs)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || inputs == nullptr) return ENTROPY_ERROR_INVALID_ARGUMENT;
    *inputs = state->engine->requiredProgramInputs();
    return ENTROPY_OK;
}
// Additive processing entry: same samples/algorithm, synchronous block identity.
EntropyResult ENTROPY_ABI_CALL processProgramBlock(EntropyModuleHandle handle,
    const EntropyAudioBlockF64* block, const EntropyProcessContext* context,
    const EntropyCrossoverProgramInputs* inputs, EntropyCrossoverProgramBlock* status)
{
    auto* state = stateFrom(handle);
    if (status == nullptr || status->struct_size < sizeof(*status))
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    *status = {};
    status->struct_size = sizeof(*status);
    if (state == nullptr || block == nullptr || block->struct_size < sizeof(*block) ||
        block->frames == 0 || block->frames != state->prepare.maximum_frames)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    if(inputs != nullptr && (inputs->struct_size < sizeof(*inputs) || (inputs->output_source_mask & ~0xffffu)!=0)) return ENTROPY_ERROR_INVALID_ARGUMENT;
    if(inputs != nullptr) state->engine->setProgramInputEpochs(inputs->music_session, inputs->music_generation, inputs->cinema_generation, inputs->output_source_mask);
    const auto before = state->engine->programBlockState();
    const auto result = processModule(handle, block, context);
    if (result != ENTROPY_OK) return result;
    const auto applied = state->engine->programBlockState();
    if (applied.sequence == 0 || (applied.session == before.session && applied.sequence <= before.sequence)) return ENTROPY_ERROR_INVALID_STATE;
    status->program = applied.program;
    status->required_inputs = applied.requiredInputs;
    status->frames = block->frames;
    status->session = applied.session;
    status->block_sequence = applied.sequence;
    status->program_revision = applied.revision;
    status->output_revision = applied.outputToken>>1;
    status->output_program = (uint32_t)(applied.outputToken&1u);
    status->first_transition_frame = applied.firstTransitionFrame;
    status->transitions_in_block = applied.transitions;
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL latency(EntropyModuleHandle handle, uint32_t* samples)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || samples == nullptr) return ENTROPY_ERROR_INVALID_ARGUMENT;
    *samples = static_cast<uint32_t> (std::max(0, state->engine->getReportedFirLatencySamples()));
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL getInputCount(uint32_t* count)
{
    if (count == nullptr) return ENTROPY_ERROR_INVALID_ARGUMENT;
    *count = inputCount;
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL getInputDescriptor(uint32_t index, EntropyCrossoverInputDescriptor* descriptor)
{
    if (index >= inputCount || descriptor == nullptr || descriptor->struct_size < sizeof(*descriptor))
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    const auto definition = inputs[index];
    *descriptor = {};
    descriptor->struct_size = sizeof(*descriptor);
    descriptor->slot_index = index;
    descriptor->semantic_id = definition.semantic;
    descriptor->program = definition.program;
    descriptor->program_channel_index = definition.programChannel;
    copyText(descriptor->identifier, sizeof(descriptor->identifier), definition.identifier);
    copyText(descriptor->label, sizeof(descriptor->label), definition.label);
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL getOutputCount(uint32_t* count)
{
    if (count == nullptr) return ENTROPY_ERROR_INVALID_ARGUMENT;
    *count = outputCount;
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL getOutputDescriptor(EntropyModuleHandle handle, uint32_t index,
                                                   EntropyCrossoverOutputDescriptor* descriptor)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || index >= outputCount || descriptor == nullptr ||
        descriptor->struct_size < sizeof(*descriptor))
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    const auto role = static_cast<uint32_t> (state->engine->roleForChannel(static_cast<int> (index)));
    const auto labels = state->engine->activeOutputLabels();
    uint32_t semantic = 0, way = 0;
    outputRole(role, semantic, way);
    *descriptor = {};
    descriptor->struct_size = sizeof(*descriptor);
    descriptor->slot_index = index;
    descriptor->active = !labels[static_cast<int> (index)].isEmpty() ? 1u : 0u;
    descriptor->semantic_role = semantic;
    descriptor->way_number = way;
    descriptor->engine_role_id = role;
    descriptor->program = state->engine->getAPVTS().getRawParameterValue("cinemaSource")->load() >= 0.5f ? 1u : 0u;
    const auto identifier = "CROSSOVER_OUT_" + entropy::native::String(index + 1);
    copyText(descriptor->identifier, sizeof(descriptor->identifier), identifier.toRawUTF8());
    copyText(descriptor->label, sizeof(descriptor->label), labels[static_cast<int> (index)].toRawUTF8());
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL getProgram(EntropyModuleHandle handle, uint32_t* program)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || program == nullptr) return ENTROPY_ERROR_INVALID_ARGUMENT;
    *program = state->engine->getAPVTS().getRawParameterValue("cinemaSource")->load() >= 0.5f ? 1u : 0u;
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL setProgram(EntropyModuleHandle handle, uint32_t program)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || program > ENTROPY_CROSSOVER_PROGRAM_CINEMA)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    return setParameterValue(*state, "cinemaSource", static_cast<double> (program));
}

EntropyResult ENTROPY_ABI_CALL getParameter(EntropyModuleHandle handle, const char* identifier, double* value)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || identifier == nullptr || value == nullptr)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    const auto* raw = state->engine->getAPVTS().getRawParameterValue(identifier);
    if (raw == nullptr) return ENTROPY_ERROR_UNSUPPORTED;
    *value = raw->load();
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL setParameter(EntropyModuleHandle handle, const char* identifier, double value)
{
    auto* state = stateFrom(handle);
    if (state == nullptr) return ENTROPY_ERROR_INVALID_ARGUMENT;
    return setParameterValue(*state, identifier, value);
}

EntropyResult ENTROPY_ABI_CALL exportState(EntropyModuleHandle handle, void* bytes, uint32_t* inOutByteCount)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || inOutByteCount == nullptr) return ENTROPY_ERROR_INVALID_ARGUMENT;
    try
    {
        entropy::native::MemoryBlock block;
        state->engine->getStateInformation(block);
        if (block.getSize() > std::numeric_limits<uint32_t>::max()) return ENTROPY_ERROR_INVALID_STATE;
        const auto required = static_cast<uint32_t> (block.getSize());
        if (bytes == nullptr) { *inOutByteCount = required; return ENTROPY_OK; }
        if (*inOutByteCount < required) { *inOutByteCount = required; return ENTROPY_ERROR_INVALID_ARGUMENT; }
        std::memcpy(bytes, block.getData(), required);
        *inOutByteCount = required;
        return ENTROPY_OK;
    }
    catch (...) { return ENTROPY_ERROR_INVALID_STATE; }
}

EntropyResult ENTROPY_ABI_CALL getParameterCount(uint32_t* count)
{
    if (count == nullptr) return ENTROPY_ERROR_INVALID_ARGUMENT;
    *count = static_cast<uint32_t>(entropy::standalone::parameterCount);
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL getParameterDescriptor(
    EntropyModuleHandle handle, uint32_t index, EntropyCrossoverParameterDescriptor* descriptor)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || descriptor == nullptr || descriptor->struct_size < sizeof(*descriptor) ||
        index >= static_cast<uint32_t>(entropy::standalone::parameterCount))
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    const auto* parameter = state->engine->getAPVTS().getParameter(
        entropy::standalone::parameterIds[index]);
    const auto* ranged = dynamic_cast<const entropy::native::RangedAudioParameter*>(parameter);
    if (ranged == nullptr) return ENTROPY_ERROR_UNSUPPORTED;
    const auto range = ranged->getNormalisableRange();
    *descriptor = {};
    descriptor->struct_size = sizeof(*descriptor);
    descriptor->index = index;
    descriptor->minimum_value = range.start;
    descriptor->maximum_value = range.end;
    descriptor->default_value = ranged->convertFrom0to1(ranged->getDefaultValue());
    descriptor->choice_count = 0;
    descriptor->kind = ENTROPY_CROSSOVER_PARAMETER_CONTINUOUS;
    if (dynamic_cast<const entropy::native::AudioParameterBool*>(parameter) != nullptr)
        descriptor->kind = ENTROPY_CROSSOVER_PARAMETER_BOOLEAN;
    else if (const auto* choice = dynamic_cast<const entropy::native::AudioParameterChoice*>(parameter))
    {
        descriptor->kind = ENTROPY_CROSSOVER_PARAMETER_CHOICE;
        descriptor->choice_count = static_cast<uint32_t>(choice->choices.size());
    }
    copyText(descriptor->identifier, sizeof(descriptor->identifier),
             entropy::standalone::parameterIds[index]);
    const auto name = parameter->getName(95);
    copyText(descriptor->label, sizeof(descriptor->label), name.toRawUTF8());
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL getTelemetry(EntropyModuleHandle handle,
                                             EntropyCrossoverTelemetry* telemetry)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || telemetry == nullptr || telemetry->struct_size < sizeof(*telemetry))
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    *telemetry = {};
    telemetry->struct_size = sizeof(*telemetry);
    telemetry->latency_samples = static_cast<uint32_t>((std::max)(0, state->engine->getReportedFirLatencySamples()));
    telemetry->block_size = static_cast<uint32_t>((std::max)(0, state->engine->getRuntimeBlockSize()));
    telemetry->sample_rate = state->engine->getRuntimeSampleRate();
    telemetry->cinema_program_active = state->engine->isCinemaBusEnabled() ? 1u : 0u;
    for (int channel = 0; channel < 3; ++channel)
        telemetry->music_input_peaks[channel] = state->engine->getMainInputPeak(channel);
    for (int channel = 0; channel < 10; ++channel)
        telemetry->cinema_input_peaks[channel] = state->engine->getCinemaInputPeak(channel);
    for (int channel = 0; channel < 16; ++channel)
    {
        telemetry->output_peaks[channel] = state->engine->getOutputPeak(channel);
        // Retained as zeroed ABI fields for v1 binary compatibility. Bandwidth
        // protection has no level detector, gain reduction or spectral meter.
        telemetry->protection_gain_reduction_db[channel] = 0.0;
        telemetry->protection_raw_peaks[channel] = 0.0;
        telemetry->protection_below_db[channel] = 0.0;
        telemetry->protection_above_db[channel] = 0.0;
        telemetry->protection_state[channel] = static_cast<uint32_t>(state->engine->getProtectionState(channel));
    }
    return ENTROPY_OK;
}

double parameterValue(const CrossoverState& state, const std::string& identifier,
                      double fallback = 0.0)
{
    if (const auto* value = state.engine->getAPVTS().getRawParameterValue(identifier))
        return value->load();
    return fallback;
}

EntropyResult ENTROPY_ABI_CALL getProtectionOutputConfig(
    EntropyModuleHandle handle, uint32_t outputIndex,
    EntropyCrossoverProtectionConfig* config)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || config == nullptr || config->struct_size < sizeof(*config) ||
        outputIndex >= outputCount)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    const auto suffix = std::to_string(outputIndex + 1u);
    *config = {};
    config->struct_size = sizeof(*config);
    config->output_index = outputIndex;
    config->enabled = parameterValue(*state, "protectorOn" + suffix) >= 0.5 ? 1u : 0u;
    config->safety_min_hz = parameterValue(*state, "safetyMinHz" + suffix, 2.0);
    config->safety_max_hz = parameterValue(*state, "safetyMaxHz" + suffix, 96000.0);
    config->trip_hold_ms = parameterValue(*state, "tripHoldMs" + suffix, 100.0);
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL setProtectionOutputConfig(
    EntropyModuleHandle handle, const EntropyCrossoverProtectionConfig* config)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || config == nullptr || config->struct_size < sizeof(*config) ||
        config->output_index >= outputCount || config->enabled > 1u ||
        ! std::isfinite(config->safety_min_hz) ||
        ! std::isfinite(config->safety_max_hz) ||
        ! std::isfinite(config->trip_hold_ms) ||
        config->safety_min_hz < 2.0 || config->safety_max_hz > 96000.0 ||
        config->safety_min_hz > config->safety_max_hz ||
        config->trip_hold_ms < 5.0 || config->trip_hold_ms > 1000.0)
        return ENTROPY_ERROR_INVALID_ARGUMENT;

    const auto suffix = std::to_string(config->output_index + 1u);
    const std::array<std::pair<std::string, double>, 4> values {{
        { "protectorOn" + suffix, static_cast<double>(config->enabled) },
        { "safetyMinHz" + suffix, config->safety_min_hz },
        { "safetyMaxHz" + suffix, config->safety_max_hz },
        { "tripHoldMs" + suffix, config->trip_hold_ms }
    }};
    for (const auto& [identifier, value] : values)
        if (assignParameterValue(*state, identifier.c_str(), value) != ENTROPY_OK)
            return ENTROPY_ERROR_INVALID_STATE;
    state->engine->serviceUpdates();
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL resetProtectionTrip(
    EntropyModuleHandle handle, uint32_t outputIndex)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || outputIndex >= outputCount)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    if (state->controlTransactionOpen)
    {
        hashBytes(state->controlTransaction.payloadHash, &outputIndex, sizeof(outputIndex));
        if (state->controlDuplicateCandidate) return ENTROPY_OK;
    }
    state->engine->resetProtectionTrip(static_cast<int>(outputIndex));
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL resetAllProtectionTrips(EntropyModuleHandle handle)
{
    auto* state = stateFrom(handle);
    if (state == nullptr) return ENTROPY_ERROR_INVALID_ARGUMENT;
    if (state->controlTransactionOpen)
    {
        constexpr uint32_t allOutputs = UINT32_MAX;
        hashBytes(state->controlTransaction.payloadHash, &allOutputs, sizeof(allOutputs));
        if (state->controlDuplicateCandidate) return ENTROPY_OK;
    }
    for (uint32_t outputIndex = 0; outputIndex < outputCount; ++outputIndex)
        state->engine->resetProtectionTrip(static_cast<int>(outputIndex));
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL getProtectionSummary(
    EntropyModuleHandle handle, EntropyCrossoverProtectionSummary* summary)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || summary == nullptr || summary->struct_size < sizeof(*summary))
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    *summary = {};
    summary->struct_size = sizeof(*summary);
    summary->output_count = outputCount;
    for (uint32_t outputIndex = 0; outputIndex < outputCount; ++outputIndex)
    {
        const auto bit = 1u << outputIndex;
        const auto suffix = std::to_string(outputIndex + 1u);
        const auto protectionState = static_cast<uint32_t>(
            state->engine->getProtectionState(static_cast<int>(outputIndex)));
        if (parameterValue(*state, "protectorOn" + suffix) >= 0.5)
            summary->enabled_mask |= bit;
        if (protectionState == ENTROPY_CROSSOVER_PROTECTION_ARMED_WARNING ||
            protectionState == ENTROPY_CROSSOVER_PROTECTION_DISABLED_WARNING)
            summary->warning_mask |= bit;
        if (protectionState == ENTROPY_CROSSOVER_PROTECTION_ARMED_TRIPPED)
            summary->tripped_mask |= bit;
    }
    summary->global_tripped = summary->tripped_mask != 0u ? 1u : 0u;
    return ENTROPY_OK;
}

double responseMagnitude(const CrossoverState& state, int channel, double frequency)
{
    const auto suffix = std::to_string(channel + 1);
    const bool hpOn = parameterValue(state, "hpOn" + suffix) >= 0.5;
    const bool lpOn = parameterValue(state, "lpOn" + suffix) >= 0.5;
    const double hpHz = (std::max)(2.0, parameterValue(state, "hpHz" + suffix, 2.0));
    const double lpHz = (std::max)(2.0, parameterValue(state, "lpHz" + suffix, 60000.0));
    const int filterType = static_cast<int>(std::lround(
        parameterValue(state, "filterType" + suffix)));
    const int processMode = static_cast<int>(std::lround(
        parameterValue(state, "processMode" + suffix)));
    const int firIndex = (std::clamp)(static_cast<int>(std::lround(
        parameterValue(state, "firLength" + suffix))), 0, 2);
    const int firTaps = firIndex == 0 ? 2049 : (firIndex == 1 ? 4097 : 8193);
    const bool cinema = parameterValue(state, "cinemaSource") >= 0.5;
    const double gainDb = parameterValue(
        state, std::string(cinema ? "cinemaGainDb" : "musicGainDb") + suffix);
    double magnitude = std::pow(10.0, gainDb / 20.0);
    if (processMode == 1)
    {
        const double sampleRate = (std::max)(1.0, state.engine->getRuntimeSampleRate());
        const double transition = (std::max)(1.0, sampleRate / (double) firTaps);
        const auto edge = [transition](double delta) {
            return 1.0 / std::sqrt(1.0 + std::exp(
                (std::clamp)(-delta / transition * 8.0, -60.0, 60.0)));
        };
        if (hpOn) magnitude *= edge(frequency - hpHz);
        if (lpOn) magnitude *= edge(lpHz - frequency);
        return magnitude;
    }
    const auto section = [filterType](double ratio) {
        if (filterType == 0) return 1.0 / std::sqrt(1.0 + std::pow(ratio, 4.0));
        if (filterType == 1) return 1.0 / std::sqrt(1.0 + std::pow(ratio, 8.0));
        if (filterType == 2) return 1.0 / (1.0 + std::pow(ratio, 4.0));
        if (filterType == 3) return 1.0 / (1.0 + std::pow(ratio, 8.0));
        if (filterType == 4) return 1.0 / std::sqrt(1.0 + std::pow(ratio, 12.0));
        return 1.0 / std::sqrt(1.0 + std::pow(ratio, 16.0));
    };
    if (hpOn) magnitude *= section(hpHz / frequency);
    if (lpOn) magnitude *= section(frequency / lpHz);
    return magnitude;
}

EntropyResult ENTROPY_ABI_CALL requestSpectrumChannel(
    EntropyModuleHandle handle, int32_t channel)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || channel < -1 || channel >= 16)
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    state->monitoredChannel = channel;
    state->engine->setLiveSpectrumChannel(channel);
    state->viewSpectrumSmoothed.fill(-120.0f);
    state->viewSpectrumAvailable = false;
    state->viewSpectrumLastUpdateMs = -1000.0;
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL getViewTelemetry(
    EntropyModuleHandle handle, EntropyCrossoverViewTelemetry* telemetry)
{
    auto* state = stateFrom(handle);
    if (state == nullptr || telemetry == nullptr ||
        telemetry->struct_size < sizeof(*telemetry))
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    *telemetry = {};
    telemetry->struct_size = sizeof(*telemetry);
    telemetry->response_point_count = ENTROPY_CROSSOVER_RESPONSE_POINTS;
    telemetry->spectrum_point_count = ENTROPY_CROSSOVER_SPECTRUM_POINTS;
    telemetry->monitored_channel = state->monitoredChannel;
    telemetry->sample_rate = state->engine->getRuntimeSampleRate();
    telemetry->minimum_frequency_hz = 20.0;
    telemetry->maximum_frequency_hz = 60000.0;
    const double logMinimum = std::log(telemetry->minimum_frequency_hz);
    const double logMaximum = std::log(telemetry->maximum_frequency_hz);
    const double nowMs = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (nowMs - state->viewResponseLastUpdateMs >= 150.0)
    {
        for (uint32_t point = 0; point < ENTROPY_CROSSOVER_RESPONSE_POINTS; ++point)
        {
            const double unit = point / double(ENTROPY_CROSSOVER_RESPONSE_POINTS - 1u);
            const double frequency = std::exp(logMinimum + unit * (logMaximum - logMinimum));
            for (int channel = 0; channel < 16; ++channel)
            {
                const double magnitude = responseMagnitude(*state, channel, frequency);
                state->viewResponseCache[(size_t) channel][point] = static_cast<float>(
                    (std::max)(-120.0, 20.0 * std::log10((std::max)(1.0e-9, magnitude))));
            }
        }
        state->viewResponseLastUpdateMs = nowMs;
    }
    for (int channel = 0; channel < 16; ++channel)
        std::copy(state->viewResponseCache[(size_t) channel].begin(),
                  state->viewResponseCache[(size_t) channel].end(),
                  telemetry->response_db[channel]);

    if (state->monitoredChannel >= 0 &&
        nowMs - state->viewSpectrumLastUpdateMs >= 50.0 &&
        state->engine->copyLiveSpectrumSamples(
            state->viewFftData.data(), CrossoverState::viewFftSize))
    {
        for (int i=0; i<CrossoverState::viewFftSize; ++i)
            state->viewFftInput[i] = static_cast<double>(state->viewFftData[i]) * state->viewWindow[i];
        state->viewFft.magnitudes(state->viewFftInput.data(), state->viewFftMagnitude.data());
        constexpr float amplitudeScale = 4.0f / CrossoverState::viewFftSize;
        for (int bin = 0; bin <= CrossoverState::viewFftSize / 2; ++bin)
        {
            const float db = entropy::native::Decibels::gainToDecibels(
                (std::max)(1.0e-9f, static_cast<float>(state->viewFftMagnitude[(size_t) bin] * amplitudeScale)),
                -120.0f);
            auto& smooth = state->viewSpectrumSmoothed[(size_t) bin];
            smooth = (std::max)(db, smooth - 1.5f);
        }
        const double nyquist = (std::max)(1.0, telemetry->sample_rate * 0.5);
        for (uint32_t point = 0; point < ENTROPY_CROSSOVER_SPECTRUM_POINTS; ++point)
        {
            const double unit = point / double(ENTROPY_CROSSOVER_SPECTRUM_POINTS - 1u);
            const double frequency = std::exp(logMinimum + unit * (logMaximum - logMinimum));
            state->viewSpectrumFrequencyCache[point] = static_cast<float>(frequency);
        }
        constexpr int lastFftBin = CrossoverState::viewFftSize / 2;
        for (uint32_t point = 0; point < ENTROPY_CROSSOVER_SPECTRUM_POINTS; ++point)
        {
            const double centre = state->viewSpectrumFrequencyCache[point];
            const double lower = point == 0
                ? telemetry->minimum_frequency_hz
                : std::sqrt(static_cast<double>(state->viewSpectrumFrequencyCache[point - 1])
                            * centre);
            const double upper = point + 1 == ENTROPY_CROSSOVER_SPECTRUM_POINTS
                ? telemetry->maximum_frequency_hz
                : std::sqrt(centre
                            * static_cast<double>(state->viewSpectrumFrequencyCache[point + 1]));
            int firstBin = (std::clamp)(static_cast<int>(std::ceil(
                lower / nyquist * lastFftBin)), 0, lastFftBin);
            int finalBin = (std::clamp)(static_cast<int>(std::floor(
                upper / nyquist * lastFftBin)), 0, lastFftBin);
            if (firstBin > finalBin)
            {
                firstBin = (std::clamp)(static_cast<int>(std::lround(
                    centre / nyquist * lastFftBin)), 0, lastFftBin);
                finalBin = firstBin;
            }
            float bucketPeak = -120.0f;
            for (int bin = firstBin; bin <= finalBin; ++bin)
                bucketPeak = (std::max)(bucketPeak,
                    state->viewSpectrumSmoothed[(size_t) bin]);
            state->viewSpectrumDbCache[point] = bucketPeak;
        }
        state->viewSpectrumAvailable = true;
        state->viewSpectrumLastUpdateMs = nowMs;
    }
    if (state->viewSpectrumAvailable)
    {
        std::copy(state->viewSpectrumFrequencyCache.begin(),
                  state->viewSpectrumFrequencyCache.end(),
                  telemetry->spectrum_frequency_hz);
        std::copy(state->viewSpectrumDbCache.begin(),
                  state->viewSpectrumDbCache.end(),
                  telemetry->spectrum_db);
        telemetry->spectrum_available = 1u;
    }
    return ENTROPY_OK;
}

EntropyResult ENTROPY_ABI_CALL queryExtension(const char* identifier, uint32_t requestedVersion,
                                              void* extensionApi, uint32_t extensionApiSize)
{
    if (identifier == nullptr || extensionApi == nullptr)
        return ENTROPY_ERROR_UNSUPPORTED;
    if (std::strcmp(identifier, ENTROPY_CROSSOVER_CONTROL_STATUS_EXTENSION_ID) == 0)
    {
        if (requestedVersion != ENTROPY_CROSSOVER_CONTROL_STATUS_EXTENSION_VERSION)
            return ENTROPY_ERROR_ABI_MISMATCH;
        if (extensionApiSize < sizeof(EntropyCrossoverControlStatusApiV1))
            return ENTROPY_ERROR_INVALID_ARGUMENT;
        auto* api = static_cast<EntropyCrossoverControlStatusApiV1*>(extensionApi);
        *api = {};
        api->struct_size = sizeof(*api);
        api->extension_version = ENTROPY_CROSSOVER_CONTROL_STATUS_EXTENSION_VERSION;
        api->begin_command = beginControlCommand;
        api->commit_command = commitControlCommand;
        api->cancel_command = cancelControlCommand;
        api->get_command_status = getControlCommandStatus;
        return ENTROPY_OK;
    }
    if (std::strcmp(identifier, ENTROPY_CROSSOVER_PROGRAM_BLOCK_EXTENSION_ID) == 0)
    {
        if (requestedVersion != ENTROPY_CROSSOVER_PROGRAM_BLOCK_EXTENSION_VERSION)
            return ENTROPY_ERROR_ABI_MISMATCH;
        if (extensionApiSize < sizeof(EntropyCrossoverProgramBlockApiV1))
            return ENTROPY_ERROR_INVALID_ARGUMENT;
        auto* api = static_cast<EntropyCrossoverProgramBlockApiV1*>(extensionApi);
        *api = {};
        api->struct_size = sizeof(*api);
        api->extension_version = ENTROPY_CROSSOVER_PROGRAM_BLOCK_EXTENSION_VERSION;
        api->process_f64 = processProgramBlock;
        api->get_required_inputs = requiredProgramInputs;
        return ENTROPY_OK;
    }
    if (std::strcmp(identifier, ENTROPY_CROSSOVER_SCHEMA_EXTENSION_ID) == 0)
    {
        if (requestedVersion != ENTROPY_CROSSOVER_SCHEMA_EXTENSION_VERSION)
            return ENTROPY_ERROR_ABI_MISMATCH;
        if (extensionApiSize < sizeof(EntropyCrossoverSchemaApiV1))
            return ENTROPY_ERROR_INVALID_ARGUMENT;
        auto* api = static_cast<EntropyCrossoverSchemaApiV1*>(extensionApi);
        *api = {};
        api->struct_size = sizeof(*api);
        api->extension_version = ENTROPY_CROSSOVER_SCHEMA_EXTENSION_VERSION;
        api->get_parameter_count = getParameterCount;
        api->get_parameter_descriptor = getParameterDescriptor;
        return ENTROPY_OK;
    }
    if (std::strcmp(identifier, ENTROPY_CROSSOVER_TELEMETRY_EXTENSION_ID) == 0)
    {
        if (requestedVersion != ENTROPY_CROSSOVER_TELEMETRY_EXTENSION_VERSION)
            return ENTROPY_ERROR_ABI_MISMATCH;
        if (extensionApiSize < sizeof(EntropyCrossoverTelemetryApiV1))
            return ENTROPY_ERROR_INVALID_ARGUMENT;
        auto* api = static_cast<EntropyCrossoverTelemetryApiV1*>(extensionApi);
        *api = {};
        api->struct_size = sizeof(*api);
        api->extension_version = ENTROPY_CROSSOVER_TELEMETRY_EXTENSION_VERSION;
        api->get_telemetry = getTelemetry;
        return ENTROPY_OK;
    }
    if (std::strcmp(identifier, ENTROPY_CROSSOVER_GESTURE_EXTENSION_ID) == 0)
    {
        if (requestedVersion != ENTROPY_CROSSOVER_GESTURE_EXTENSION_VERSION)
            return ENTROPY_ERROR_ABI_MISMATCH;
        if (extensionApiSize < sizeof(EntropyCrossoverGestureApiV1))
            return ENTROPY_ERROR_INVALID_ARGUMENT;
        auto* api = static_cast<EntropyCrossoverGestureApiV1*>(extensionApi);
        *api = {};
        api->struct_size = sizeof(*api);
        api->extension_version = ENTROPY_CROSSOVER_GESTURE_EXTENSION_VERSION;
        api->begin_parameter_gesture = beginParameterGesture;
        api->end_parameter_gesture = endParameterGesture;
        return ENTROPY_OK;
    }
    if (std::strcmp(identifier, ENTROPY_CROSSOVER_VIEW_TELEMETRY_EXTENSION_ID) == 0)
    {
        if (requestedVersion != ENTROPY_CROSSOVER_VIEW_TELEMETRY_EXTENSION_VERSION)
            return ENTROPY_ERROR_ABI_MISMATCH;
        if (extensionApiSize < sizeof(EntropyCrossoverViewTelemetryApiV1))
            return ENTROPY_ERROR_INVALID_ARGUMENT;
        auto* api = static_cast<EntropyCrossoverViewTelemetryApiV1*>(extensionApi);
        *api = {};
        api->struct_size = sizeof(*api);
        api->extension_version = ENTROPY_CROSSOVER_VIEW_TELEMETRY_EXTENSION_VERSION;
        api->request_spectrum_channel = requestSpectrumChannel;
        api->get_view_telemetry = getViewTelemetry;
        return ENTROPY_OK;
    }
    if (std::strcmp(identifier, ENTROPY_CROSSOVER_PROTECTION_EXTENSION_ID) == 0)
    {
        if (requestedVersion != ENTROPY_CROSSOVER_PROTECTION_EXTENSION_VERSION)
            return ENTROPY_ERROR_ABI_MISMATCH;
        if (extensionApiSize < sizeof(EntropyCrossoverProtectionApiV1))
            return ENTROPY_ERROR_INVALID_ARGUMENT;
        auto* api = static_cast<EntropyCrossoverProtectionApiV1*>(extensionApi);
        *api = {};
        api->struct_size = sizeof(*api);
        api->extension_version = ENTROPY_CROSSOVER_PROTECTION_EXTENSION_VERSION;
        api->get_output_config = getProtectionOutputConfig;
        api->set_output_config = setProtectionOutputConfig;
        api->reset_trip = resetProtectionTrip;
        api->reset_all_trips = resetAllProtectionTrips;
        api->get_summary = getProtectionSummary;
        return ENTROPY_OK;
    }
    if (std::strcmp(identifier, ENTROPY_CROSSOVER_EXTENSION_ID) != 0)
        return ENTROPY_ERROR_UNSUPPORTED;
    if (requestedVersion != ENTROPY_CROSSOVER_EXTENSION_VERSION)
        return ENTROPY_ERROR_ABI_MISMATCH;
    if (extensionApiSize < sizeof(EntropyCrossoverApiV1))
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    auto* api = static_cast<EntropyCrossoverApiV1*> (extensionApi);
    *api = {};
    api->struct_size = sizeof(*api);
    api->extension_version = ENTROPY_CROSSOVER_EXTENSION_VERSION;
    api->get_input_count = getInputCount;
    api->get_input_descriptor = getInputDescriptor;
    api->get_output_count = getOutputCount;
    api->get_output_descriptor = getOutputDescriptor;
    api->get_program = getProgram;
    api->set_program = setProgram;
    api->get_parameter = getParameter;
    api->set_parameter = setParameter;
    api->export_state = exportState;
    api->import_state = importState;
    return ENTROPY_OK;
}
}

extern "C" ENTROPY_ABI_EXPORT EntropyResult ENTROPY_ABI_CALL entropy_module_get_api(
    uint32_t requestedAbiVersion, EntropyModuleApiV1* api)
{
    if (api == nullptr || api->struct_size < sizeof(*api))
        return ENTROPY_ERROR_INVALID_ARGUMENT;
    if (requestedAbiVersion != ENTROPY_MODULE_ABI_VERSION)
        return ENTROPY_ERROR_ABI_MISMATCH;
    *api = {};
    api->struct_size = sizeof(*api);
    api->abi_version = ENTROPY_MODULE_ABI_VERSION;
    api->get_descriptor = getDescriptor;
    api->create = createModule;
    api->destroy = destroyModule;
    api->prepare = prepareModule;
    api->reset = resetModule;
    api->set_config = setConfig;
    api->process_f64 = processModule;
    api->latency_samples = latency;
    api->query_extension = queryExtension;
    return ENTROPY_OK;
}
