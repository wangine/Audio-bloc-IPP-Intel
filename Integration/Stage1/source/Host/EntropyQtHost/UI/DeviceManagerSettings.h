#pragma once

class QSettings;

namespace entropy::host
{
bool migrateTwoDacSettings(QSettings& settings);
}
