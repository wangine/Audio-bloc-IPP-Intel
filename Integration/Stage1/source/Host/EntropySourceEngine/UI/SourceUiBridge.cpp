#include "SourceUiBridge.h"
namespace entropy::host
{
namespace
{
SourceController *controller = nullptr;
} // namespace
void attachSourceUi(SourceController *c) noexcept { controller = c; }
bool readSourceControl(EngineControlSnapshot &s) noexcept
{
    if (!controller)
        return false;
    try
    {
        s = EngineControlSnapshot{};
        s.muteMask = controller->snapshot().state.manualMuteMask;
        return true;
    }
    catch (...)
    {
        return false;
    }
}
bool readSourceTelemetry(EngineTelemetrySnapshot &s) noexcept
{
    if (!controller)
        return false;
    try
    {
        return controller->telemetry(s);
    }
    catch (...)
    {
        return false;
    }
}
bool writeCinemaSrcEngine(uint32_t engine) noexcept {
    if(!controller || (engine!=0 && engine!=2))return false;
    try { SourceCommand command;command.type=SourceCommand::Engine;command.role=2;command.value=engine;return controller->command(command); }
    catch(...) { return false; }
}
bool writeSourceMuteMask(uint32_t mask) noexcept
{
    if (!controller)
        return false;
    controller->mute(mask);
    return true;
}
bool readSourceDemandMask(uint32_t& mask) noexcept
{
    if (!controller) return false;
    try { mask = controller->snapshot().demandSourceMask; return true; }
    catch (...) { return false; }
}
} // namespace entropy::host
