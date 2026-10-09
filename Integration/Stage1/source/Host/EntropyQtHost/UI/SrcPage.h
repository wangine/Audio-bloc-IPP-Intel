#pragma once

class QWidget;

namespace entropy::host
{
struct ModuleViewContext;
QWidget* createSrcView(const ModuleViewContext& context, QWidget* owner);
}
