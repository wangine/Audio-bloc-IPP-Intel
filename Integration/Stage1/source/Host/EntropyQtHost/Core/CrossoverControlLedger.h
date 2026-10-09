#pragma once

#include <entropy/crossover_abi.h>
#include <entropy/module_abi.h>

#include <chrono>
#include <array>
#include <cstdint>

namespace entropy::host
{
struct CrossoverCommandId
{
    std::uint64_t session = 0;
    std::uint64_t sequence = 0;
    friend bool operator==(const CrossoverCommandId&, const CrossoverCommandId&) = default;
};

struct CrossoverCommandSnapshot
{
    CrossoverCommandId requested{};
    CrossoverCommandId accepted{};
    CrossoverCommandId applied{};
    CrossoverCommandId failed{};
    std::uint32_t state = ENTROPY_CROSSOVER_COMMAND_IDLE;
    std::uint32_t reason = ENTROPY_CROSSOVER_COMMAND_REASON_NONE;
    std::uint32_t kind = 0;
    std::uint32_t shadowFailureMask = 0;
};

struct CrossoverCommandRetirement {
    std::uint64_t commandSequence=0, sourceSequence=0;
};
inline std::uint64_t crossoverAcknowledgedSequence(const CrossoverCommandSnapshot& command) noexcept {
    if(command.state==ENTROPY_CROSSOVER_COMMAND_APPLIED)return command.applied.sequence;
    if(command.state==ENTROPY_CROSSOVER_COMMAND_COMMITTED_STOPPED)return command.accepted.sequence;
    return 0;
}
inline bool crossoverCommandMayStart(const CrossoverCommandSnapshot& command,std::uint64_t finalized) noexcept {
    if(command.state==ENTROPY_CROSSOVER_COMMAND_REQUESTED || command.state==ENTROPY_CROSSOVER_COMMAND_ACCEPTED)return false;
    const auto acknowledged=crossoverAcknowledgedSequence(command);
    return acknowledged==0 || acknowledged==finalized;
}
inline CrossoverCommandRetirement retireCrossoverCommandSnapshot(const CrossoverCommandSnapshot& command,
    std::uint64_t sourceSequence,std::uint64_t& finalized) noexcept {
    const auto acknowledged=crossoverAcknowledgedSequence(command);
    if(!acknowledged || acknowledged==finalized)return {};
    finalized=acknowledged;return {acknowledged,sourceSequence};
}

struct CrossoverControlEndpoint
{
    EntropyModuleHandle handle = nullptr;
    EntropyCrossoverControlStatusApiV1 api{};

    bool available() const noexcept
    {
        return handle != nullptr &&
            api.struct_size >= sizeof(EntropyCrossoverControlStatusApiV1) &&
            api.extension_version == ENTROPY_CROSSOVER_CONTROL_STATUS_EXTENSION_VERSION &&
            api.begin_command != nullptr && api.commit_command != nullptr &&
            api.cancel_command != nullptr && api.get_command_status != nullptr;
    }

    template <typename Apply>
    EntropyResult submit(CrossoverCommandId id, std::uint32_t kind,
                         Apply&& apply) const noexcept(noexcept(apply()))
    {
        if (!available()) return apply();
        EntropyCrossoverCommandIdV1 abiId{};
        abiId.struct_size = sizeof(abiId);
        abiId.extension_version = ENTROPY_CROSSOVER_CONTROL_STATUS_EXTENSION_VERSION;
        abiId.session = id.session;
        abiId.sequence = id.sequence;
        auto result = api.begin_command(handle, &abiId, kind);
        if (result != ENTROPY_OK) return result;
        result = apply();
        if (result != ENTROPY_OK)
        {
            api.cancel_command(handle, ENTROPY_CROSSOVER_COMMAND_REASON_INVALID_PARAMETER);
            return result;
        }
        return api.commit_command(handle);
    }

    EntropyResult read(EntropyCrossoverControlStatusV1& status) const noexcept
    {
        if (!available()) return ENTROPY_ERROR_UNSUPPORTED;
        status = {};
        status.struct_size = sizeof(status);
        status.extension_version = ENTROPY_CROSSOVER_CONTROL_STATUS_EXTENSION_VERSION;
        return api.get_command_status(handle, &status);
    }
};

std::uint64_t createCrossoverCommandSession() noexcept;

inline bool crossoverAuthoritySucceeded(
    const std::array<EntropyResult, 4>& results) noexcept
{
    return results[3] == ENTROPY_OK;
}

inline bool crossoverCommandUsesControlLane(std::uint32_t kind) noexcept
{
    return kind == ENTROPY_CROSSOVER_COMMAND_GESTURE_BEGIN ||
           kind == ENTROPY_CROSSOVER_COMMAND_GESTURE_END ||
           kind == ENTROPY_CROSSOVER_COMMAND_CONTROL_ONLY;
}

CrossoverCommandSnapshot mergeCrossoverCommandSnapshots(
    const CrossoverCommandSnapshot& data,
    const CrossoverCommandSnapshot& control) noexcept;

class CrossoverControlLedger
{
public:
    using Clock = std::chrono::steady_clock;
    static constexpr auto adoptionTimeout = std::chrono::seconds(2);

    bool request(CrossoverCommandId id, std::uint32_t kind, bool processingRunning);
    void authorityAccepted(Clock::time_point now = Clock::now()) noexcept;
    void authorityRejected(std::uint32_t reason) noexcept;
    void observeAuthority(const EntropyCrossoverControlStatusV1& status) noexcept;
    void observeShadow(std::uint32_t index, EntropyResult result) noexcept;
    void markLegacyAccepted() noexcept;
    void markStoppedCommitted() noexcept;
    void markApplicationUnconfirmed(std::uint32_t reason) noexcept;
    void markTimeout() noexcept;
    bool deadlineExpired(Clock::time_point now = Clock::now()) const noexcept;
    CrossoverCommandSnapshot snapshot() const noexcept { return value_; }

private:
    bool current(CrossoverCommandId id) const noexcept { return value_.requested == id; }
    static CrossoverCommandId idFrom(const EntropyCrossoverCommandIdV1&) noexcept;
    bool terminal() const noexcept;

    CrossoverCommandSnapshot value_{};
    bool processingRunning_ = false;
    Clock::time_point deadline_{};
};
}
