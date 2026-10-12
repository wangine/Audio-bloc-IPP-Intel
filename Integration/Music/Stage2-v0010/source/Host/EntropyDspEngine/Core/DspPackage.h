#pragma once
#include <filesystem>
#include <string>
namespace entropy::stage {bool verifyDspPackage(const std::filesystem::path&,std::string& error,bool requireCenter=false,bool requireAligner=false,bool requireEq=false);}
