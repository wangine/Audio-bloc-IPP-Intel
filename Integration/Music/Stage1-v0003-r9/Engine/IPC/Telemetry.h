#pragma once
#include "stage1_control_v1.h"
#include <filesystem>
namespace entropy::music {
void writeDiagnostic(const std::filesystem::path &, const Stage1ControlResponse &);
}
