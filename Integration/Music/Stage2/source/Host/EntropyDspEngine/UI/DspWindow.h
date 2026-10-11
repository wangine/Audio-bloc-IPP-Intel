#pragma once
#include "DspController.h"
#include <QMainWindow>
#include <filesystem>
namespace entropy::stage {
class DspWindow:public QMainWindow {
public:DspWindow(DspController&,const std::filesystem::path& runtimeRoot,bool startAutomatic=true);
};
}
