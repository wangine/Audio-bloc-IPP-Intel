#pragma once
#include "Client/EngineFacade.h"
#include "RenderOverview.h"
#include <QMainWindow>
#include <QDialog>
#include <QLabel>
#include <QPointer>
namespace entropy::stage {
enum class RenderPage:unsigned {Crossover,Matrix,Headroom,Devices};
class RenderWindow final:public QMainWindow {
public:
 explicit RenderWindow(entropy::ui::EngineFacade&);
 void openModule(RenderPage);
 QDialog* moduleDialog(RenderPage p) const {return dialogs_[unsigned(p)];}
 GraphNodeFrame* node(RenderPage p) const {return nodes_[unsigned(p)];}
private:
 friend struct RenderUiTestAccess;
 void refresh();
 entropy::ui::EngineFacade& controller_;
 std::array<GraphNodeFrame*,4> nodes_{};
 std::array<QPointer<QDialog>,4> dialogs_{};
 std::unique_ptr<entropy::host::EngineTelemetrySnapshot> snapshot_=std::make_unique<entropy::host::EngineTelemetrySnapshot>();
 std::array<RenderOverview*,5> panels_{};
 QPushButton* programShortcut_=nullptr;
 QPushButton* outputShortcut_=nullptr;
 QLabel* connection_=nullptr;
 QLabel* latency_=nullptr;
};
}
