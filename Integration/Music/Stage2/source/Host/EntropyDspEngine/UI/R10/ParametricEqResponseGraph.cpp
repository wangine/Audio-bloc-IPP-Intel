#include "ParametricEqResponseGraph.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>

namespace entropy::host
{
namespace
{
constexpr std::array<const char*, 8> colours{{
    "#ff4c63", "#f6a13a", "#36df8d", "#29cae8",
    "#2f9ef5", "#bd6bf1", "#cbd6df", "#ff5ca4"}};
}

ParametricEqResponseGraph::ParametricEqResponseGraph(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(210);
    setMouseTracking(true);
}

void ParametricEqResponseGraph::setModel(ParametricEqModel* model) noexcept
{
    model_ = model;
    update();
}

void ParametricEqResponseGraph::setEditCallback(std::function<void()> callback)
{
    editCallback_ = std::move(callback);
}

void ParametricEqResponseGraph::setSelectionCallback(
    std::function<void(int)> callback)
{
    selectionCallback_ = std::move(callback);
}

void ParametricEqResponseGraph::armBandForWheel(int band) noexcept
{
    wheelArmedBand_ = band >= 0 && band < 8 ? band : -1;
    update();
}

QRectF ParametricEqResponseGraph::plotRect() const noexcept
{
    return rect().adjusted(52, 18, -18, -34);
}

QPointF ParametricEqResponseGraph::nodePosition(int band) const noexcept
{
    if (model_ == nullptr || band < 0 || band >= 8) return {};
    const auto& value = model_->workingSettings().bands[band];
    const auto plot = plotRect();
    const double normalX = std::log10(std::clamp(value.frequency_hz,20.0,20000.0)/20.0) /
                           std::log10(1000.0);
    const double normalY = (12.0 - std::clamp(value.static_gain_db,-12.0,12.0))/24.0;
    return {plot.left()+normalX*plot.width(), plot.top()+normalY*plot.height()};
}

int ParametricEqResponseGraph::hitTestNode(const QPointF& position) const noexcept
{
    if (model_ == nullptr) return -1;
    for (int band = 0; band < 8; ++band)
        if (QLineF(position, nodePosition(band)).length() <= 14.0) return band;
    return -1;
}

double ParametricEqResponseGraph::xToFrequency(double x) const noexcept
{
    const auto plot=plotRect();
    const double normal=std::clamp((x-plot.left())/plot.width(),0.0,1.0);
    return 20.0*std::pow(1000.0,normal);
}

double ParametricEqResponseGraph::yToGain(double y) const noexcept
{
    const auto plot=plotRect();
    const double normal=std::clamp((y-plot.top())/plot.height(),0.0,1.0);
    return 12.0-24.0*normal;
}

void ParametricEqResponseGraph::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor("#07141d"));
    const auto plot=plotRect();
    painter.setPen(QPen(QColor("#17394a"),1));
    for(double gain:{-12.0,-6.0,0.0,6.0,12.0}){
        const double y=plot.top()+(12.0-gain)/24.0*plot.height();
        painter.drawLine(QPointF(plot.left(),y),QPointF(plot.right(),y));
        painter.setPen(QColor("#89aaba"));
        painter.drawText(QRectF(2,y-10,44,20),Qt::AlignRight|Qt::AlignVCenter,
                         QString::number(gain,'f',0));
        painter.setPen(QPen(QColor("#17394a"),1));
    }
    for(double frequency:{20.0,50.0,100.0,200.0,500.0,1000.0,2000.0,5000.0,10000.0,20000.0}){
        const double x=plot.left()+std::log10(frequency/20.0)/std::log10(1000.0)*plot.width();
        painter.drawLine(QPointF(x,plot.top()),QPointF(x,plot.bottom()));
        painter.setPen(QColor("#89aaba"));
        const QString label=frequency>=1000.0?QString::number(frequency/1000.0,'g',2)+"k":QString::number(frequency,'f',0);
        painter.drawText(QRectF(x-24,plot.bottom()+5,48,20),Qt::AlignHCenter,label);
        painter.setPen(QPen(QColor("#17394a"),1));
    }
    if(model_==nullptr||!model_->schemaValid())return;
    QPainterPath curve;
    const int points=std::max(2,static_cast<int>(plot.width()));
    for(int index=0;index<points;++index){
        const double normal=static_cast<double>(index)/(points-1);
        const double frequency=20.0*std::pow(1000.0,normal);
        const double gain=std::clamp(parametricEqMagnitudeDb(model_->workingSettings(),frequency,192000.0),-12.0,12.0);
        const QPointF point(plot.left()+normal*plot.width(),plot.top()+(12.0-gain)/24.0*plot.height());
        if(index==0)curve.moveTo(point);else curve.lineTo(point);
    }
    painter.setPen(QPen(QColor("#49d9ff"),2));
    painter.drawPath(curve);
    for(int band=0;band<8;++band){
        const QPointF point=nodePosition(band);const QColor colour(colours[band]);
        painter.setBrush(QColor("#07141d"));painter.setPen(QPen(colour,band==model_->selectedBand()?3:2));
        painter.drawEllipse(point,band==model_->selectedBand()?10:8,band==model_->selectedBand()?10:8);
        painter.setPen(colour);painter.drawText(QRectF(point.x()-10,point.y()-10,20,20),Qt::AlignCenter,QString::number(band+1));
    }
}

void ParametricEqResponseGraph::mousePressEvent(QMouseEvent* event)
{
    draggedBand_=hitTestNode(event->position());
    if(draggedBand_>=0&&model_){wheelArmedBand_=draggedBand_;model_->selectBand(draggedBand_);if(selectionCallback_)selectionCallback_(draggedBand_);update();}
}

void ParametricEqResponseGraph::mouseMoveEvent(QMouseEvent* event)
{
    if(draggedBand_<0||model_==nullptr)return;
    const auto prefix="band"+std::to_string(draggedBand_)+".";
    model_->edit(prefix+"frequencyHz",xToFrequency(event->position().x()));
    model_->edit(prefix+"staticGainDb",yToGain(event->position().y()));
    if(editCallback_)editCallback_();update();
}

void ParametricEqResponseGraph::mouseReleaseEvent(QMouseEvent*){draggedBand_=-1;}

void ParametricEqResponseGraph::wheelEvent(QWheelEvent* event)
{
    if(model_==nullptr){event->ignore();return;}const int band=hitTestNode(event->position());
    if(band<0||band!=wheelArmedBand_||band!=model_->selectedBand()){event->ignore();return;}
    const auto prefix="band"+std::to_string(band)+".";const auto& current=model_->workingSettings().bands[band];
    const double next=std::clamp(current.q*(event->angleDelta().y()>0?1.08:1.0/1.08),0.1,20.0);
    if(model_->edit(prefix+"q",next)){model_->selectBand(band);if(editCallback_)editCallback_();update();event->accept();}
}
} // namespace entropy::host
