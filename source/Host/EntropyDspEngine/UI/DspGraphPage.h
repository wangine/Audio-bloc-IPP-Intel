#pragma once
#include "DspController.h"
#include "DspGraphNodes.h"
#include <QWidget>
#include <QDialog>
#include <QLabel>
#include <array>
namespace entropy::stage {
enum class DspModuleKind:unsigned {Loudness,Eq,Center,Dd,Alignment};
class DspGraphPage:public QWidget {
public:
 explicit DspGraphPage(const std::filesystem::path& runtimeRoot,QWidget* parent=nullptr);
 void refresh(const DspUiSnapshot&,uint64_t now);
 void bindController(DspController& controller) {controller_=&controller;}
 void openModule(DspModuleKind);
 void resetProfileViews();
 GraphNodeFrame* moduleNode(DspModuleKind k) const {return modules_[static_cast<unsigned>(k)];}
 MatrixGraphNode* inputMatrix() const {return input_;}
 MatrixGraphNode* outputMatrix() const {return output_;}
 QDialog* moduleDialog(DspModuleKind k) const {return dialogs_[static_cast<unsigned>(k)];}
private:
 DspController* controller_=nullptr;
 MatrixGraphNode* input_=nullptr;MatrixGraphNode* output_=nullptr;QLabel* status_=nullptr;
 std::array<GraphNodeFrame*,5> modules_{};std::array<QDialog*,5> dialogs_{};
};
}
