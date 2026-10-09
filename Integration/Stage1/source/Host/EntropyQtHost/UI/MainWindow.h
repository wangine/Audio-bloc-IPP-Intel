#pragma once

#include "Core/EndpointProbe.h"
#include "Core/GraphManifest.h"
#include "Core/GraphRuntime.h"
#include "Core/LiveInputProbe.h"
#include "Core/ModuleRegistry.h"
#include "Core/SyntheticGraphProbe.h"

#include <QMainWindow>

#include <filesystem>

namespace entropy::host
{
class MainWindow final : public QMainWindow
{
public:
    MainWindow(const ModuleRegistry& registry,
               const ManifestReport& report,
               const GraphRuntime& runtime,
               const SyntheticProbeReport& probe,
               const EndpointProbeReport& endpoints,
               const LiveInputProbeReport& liveInput,
               const LiveInputProbeReport& liveCinema,
               const std::string& runtimeError,
               const std::filesystem::path& moduleRoot);
};
} // namespace entropy::host
