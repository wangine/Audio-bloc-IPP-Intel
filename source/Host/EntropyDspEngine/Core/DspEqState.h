#pragma once
#include "DspEqModule.h"
#include <QJsonObject>
namespace entropy::stage {
QJsonObject eqToJson(const EqSettings&);
bool eqFromJson(const QJsonObject&,EqSettings&);
}
