#pragma once
#include <windows.h>
#include <filesystem>
#include <string>
namespace entropy::stage {
enum class NativeModuleId {CenterV2,LrcAligner,ParametricEq};
struct NativeModuleRecord {const char* filename;const char* identifier;const char* sha256;};
const NativeModuleRecord* nativeModule(NativeModuleId) noexcept;
HMODULE loadNativeModule(NativeModuleId,const std::filesystem::path&,std::string&);
}
