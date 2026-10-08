#pragma once

#include <QWidget>

namespace entropy::host
{
// Presentation-only factory. Selection is performed by ModuleViewRegistry using
// the stable module id; this View never loads or owns a DSP module instance.
QWidget* createCrossoverView(QWidget* owner, bool openBandwidthProtector = false);
}
