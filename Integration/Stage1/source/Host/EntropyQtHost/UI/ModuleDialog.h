#pragma once
#include <QDialog>
#include <QMouseEvent>
namespace entropy::host {
class ModuleDialog final:public QDialog {
public:
    using QDialog::QDialog;
protected:
    void mouseDoubleClickEvent(QMouseEvent* event)override {
        isMaximized()?showNormal():showMaximized();
        QDialog::mouseDoubleClickEvent(event);
    }
};
}
