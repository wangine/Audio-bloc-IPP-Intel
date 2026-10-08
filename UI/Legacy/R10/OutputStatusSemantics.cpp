#include "OutputStatusSemantics.h"

namespace entropy::host::ui
{
DacOutputPresentation projectDacOutputPresentation(
    bool bound, bool pinsConfigured, bool armEnabled,
    bool engineConfigured, bool sinkRunning) noexcept
{
    if (! bound) return DacOutputPresentation::unbound;
    if (! pinsConfigured) return DacOutputPresentation::boundRouteUnconfigured;
    if (engineConfigured && sinkRunning)
        return DacOutputPresentation::boundRoutedSinkLive;
    if (! armEnabled) return DacOutputPresentation::boundRoutedArmDisabled;
    return DacOutputPresentation::boundRoutedSinkClosed;
}

std::string_view dacOutputPresentationText(DacOutputPresentation state) noexcept
{
    switch (state)
    {
        case DacOutputPresentation::unbound:
            return "UNBOUND";
        case DacOutputPresentation::boundRouteUnconfigured:
            return "BOUND | ROUTE NOT CONFIGURED | ARM DISABLED";
        case DacOutputPresentation::boundRoutedArmDisabled:
            return "BOUND | ROUTED | ARM DISABLED | SINK CLOSED";
        case DacOutputPresentation::boundRoutedSinkClosed:
            return "BOUND | ROUTED | ARM ENABLED | SINK CLOSED";
        case DacOutputPresentation::boundRoutedSinkLive:
            return "BOUND | ROUTED | ARM ENABLED | SINK LIVE";
    }
    return "UNBOUND";
}

std::string_view crossoverRoleAssignmentText(bool assigned) noexcept
{
    return assigned ? "ROLE ASSIGNED" : "ROLE NOT ASSIGNED";
}
}

