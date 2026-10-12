#pragma once
#include <QWidget>
namespace entropy::host {
inline bool liveUiRefreshAllowed(const QWidget* view) noexcept {
    return view && view->isVisible() && !view->window()->isMinimized();
}
}
