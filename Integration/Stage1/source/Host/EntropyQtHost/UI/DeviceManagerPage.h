#pragma once

class QWidget;

namespace entropy::host
{
class ModuleRegistry;
QWidget* createDeviceManagerView(const ModuleRegistry& registry, QWidget* owner);
}
