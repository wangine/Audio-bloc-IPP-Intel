#pragma once
#include "EngineFacade.h"
#include <QTimer>
#include <QWidget>
namespace entropy::ui {
class StartupProfilePrompt final : public QObject {
public:
    StartupProfilePrompt(EngineFacade&,QWidget&);
    void requestSelection(){prompted_=false;offer();}
private:
    void offer();
    EngineFacade& facade_;QWidget& window_;QTimer timer_;bool prompted_=false;
};
}
