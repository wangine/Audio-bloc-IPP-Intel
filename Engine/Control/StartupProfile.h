#pragma once
#include <filesystem>
#include <string>
namespace entropy::control {
std::filesystem::path motherProfilePath();
std::string profilePathUtf8(const std::filesystem::path&);
}
