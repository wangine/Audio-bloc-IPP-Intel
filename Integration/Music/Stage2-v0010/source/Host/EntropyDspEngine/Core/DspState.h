#pragma once
#include "DspCenterModule.h"
#include "DspEqModule.h"
#include <QByteArray>
#include <QString>
#include <filesystem>
#include <string>
#include <optional>
namespace entropy::stage {
struct DspState {
 CenterSettings centerSettings{};EqSettings eqSettings=defaultEqSettings();bool centerConfigured=false;
 std::optional<bool> alignmentActive;QByteArray importedProject,importedIni;QString sourceDiscovery;
 bool load(const std::filesystem::path&,std::string&);bool save(const std::filesystem::path&,std::string&) const;
 bool loadStartup(const std::filesystem::path& root,QString& selected,std::string& warning,std::string& error);
 bool remember(const std::filesystem::path& root,const QString& selected,std::string& error) const;
};
}
