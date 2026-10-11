#pragma once
#include "DspController.h"
#include <QWidget>
namespace entropy::stage {
QWidget* createStage2CenterView(QWidget*,DspController* controller=nullptr);
void refreshStage2CenterView(QWidget*,const DspUiSnapshot&);
}
