#pragma once
#include "DspController.h"
#include <QWidget>
namespace entropy::stage {
QWidget* createRecoveryView(QWidget* parent,DspController* controller);
void refreshRecoveryView(QWidget* page,const DspUiSnapshot& snapshot);
}
