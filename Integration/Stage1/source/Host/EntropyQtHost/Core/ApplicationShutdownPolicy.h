#pragma once

#include <chrono>

namespace entropy::host
{

enum class ApplicationOutputState
{
    armed,
    gateClosed
};

enum class ApplicationShutdownAction
{
    waitForGate,
    signalEngine,
    waitForEngine,
    complete,
    timeout
};

class ApplicationShutdownPolicy final
{
public:
    [[nodiscard]] ApplicationShutdownAction advance(
        ApplicationOutputState outputState,
        bool enginePresent,
        std::chrono::milliseconds phaseElapsed) noexcept
    {
        if (phase_ == Phase::complete)
            return ApplicationShutdownAction::complete;
        if (phase_ == Phase::timedOut)
            return ApplicationShutdownAction::timeout;
        if (phaseElapsed > timeoutDuration)
        {
            phase_ = Phase::timedOut;
            return ApplicationShutdownAction::timeout;
        }

        if (phase_ == Phase::closingGate)
        {
            if (outputState != ApplicationOutputState::gateClosed)
                return ApplicationShutdownAction::waitForGate;

            phase_ = Phase::waitingForEngine;
            return ApplicationShutdownAction::signalEngine;
        }

        if (enginePresent)
            return ApplicationShutdownAction::waitForEngine;

        phase_ = Phase::complete;
        return ApplicationShutdownAction::complete;
    }

    static constexpr std::chrono::milliseconds timeoutDuration{5000};

private:
    enum class Phase
    {
        closingGate,
        waitingForEngine,
        complete,
        timedOut
    };

    Phase phase_ = Phase::closingGate;
};

} // namespace entropy::host
