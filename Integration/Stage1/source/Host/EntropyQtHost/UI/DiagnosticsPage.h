#pragma once

#include <filesystem>

class QWidget;

namespace entropy::host
{
QWidget* createDiagnosticsView(const std::filesystem::path& moduleRoot,
                               QWidget* parent = nullptr);
} // namespace entropy::host
