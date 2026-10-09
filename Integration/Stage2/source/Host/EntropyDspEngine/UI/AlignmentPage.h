#pragma once
#include "DspController.h"
#include <QWidget>
namespace entropy::stage {
QWidget* createAlignmentView(QWidget*,DspController*);
void refreshAlignmentView(QWidget*,const DspUiSnapshot&);
QString alignmentStatus(const AlignmentSnapshot&);
}
