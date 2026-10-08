#pragma once
#include "ControlPlane.h"
#include <filesystem>
namespace entropy::control {
struct LoadedProfile {ipc::Value document;std::vector<unsigned char> crossover;ParameterSnapshot parameters;};
class ProfileStore {
public:
 void save(const std::filesystem::path&,ControlPlane&,const ipc::Value& devices);
 LoadedProfile load(const std::filesystem::path&);
 void adopt(ipc::Value document){document_=std::move(document);}
private:ipc::Value document_;
};
}
