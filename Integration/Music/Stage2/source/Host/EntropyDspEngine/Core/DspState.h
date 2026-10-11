#pragma once
#include "DspCenterModule.h"
#include "DspEqModule.h"
#include <QByteArray>
#include <QString>
#include <filesystem>
#include <string>
namespace entropy::stage {
struct DspState {CenterSettings centerSettings{};EqSettings eqSettings=defaultEqSettings();bool centerConfigured=false;QByteArray importedProject,importedIni;QString sourceDiscovery;bool load(const std::filesystem::path&,std::string&);bool save(const std::filesystem::path&,std::string&) const;};
}
