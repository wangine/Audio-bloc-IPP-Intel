#pragma once

#include <string_view>

namespace entropy::host::ui
{
enum class DacOutputPresentation
{
    unbound,
    boundRouteUnconfigured,
    boundRoutedArmDisabled,
    boundRoutedSinkClosed,
    boundRoutedSinkLive
};

DacOutputPresentation projectDacOutputPresentation(
    bool bound, bool pinsConfigured, bool armEnabled,
    bool engineConfigured, bool sinkRunning) noexcept;

std::string_view dacOutputPresentationText(DacOutputPresentation state) noexcept;
std::string_view crossoverRoleAssignmentText(bool assigned) noexcept;
}

