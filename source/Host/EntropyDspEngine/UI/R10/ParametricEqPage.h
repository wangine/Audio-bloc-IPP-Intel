#pragma once

class QWidget;
namespace entropy::stage {class DspController;}

namespace entropy::host
{
QWidget* createParametricEqView(QWidget* owner,entropy::stage::DspController* controller=nullptr);
}
