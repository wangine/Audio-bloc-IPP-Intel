#pragma once
#include "DspGraphNodes.h"
#include "Core/EngineTelemetry.h"
namespace entropy::stage {
enum class OverviewKind {Input,Crossover,Matrix,Headroom,Egress};
class RenderOverview final : public GraphNodeFrame {
public:
 RenderOverview(OverviewKind,const entropy::host::EngineTelemetrySnapshot&,QWidget* parent=nullptr);
 void refresh(bool fresh){fresh_=fresh;update();}
 bool signalFresh() const noexcept{return fresh_;}
 double inputPeak(unsigned lane) const noexcept;
 bool routeActive(unsigned source,unsigned destination) const noexcept;
protected:
 void paintEvent(QPaintEvent*) override;
private:
 OverviewKind kind_;
 const entropy::host::EngineTelemetrySnapshot& snapshot_;
 bool fresh_=false;
};
}
