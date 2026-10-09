#pragma once
#include "SourceController.h"
namespace entropy::host
{
void attachSourceUi(SourceController *) noexcept;
bool readSourceControl(EngineControlSnapshot &) noexcept;
bool readSourceTelemetry(EngineTelemetrySnapshot &) noexcept;
bool readSourceDemandMask(uint32_t &) noexcept;
bool writeCinemaSrcEngine(uint32_t) noexcept;
bool writeSourceMuteMask(uint32_t) noexcept;
} // namespace entropy::host
