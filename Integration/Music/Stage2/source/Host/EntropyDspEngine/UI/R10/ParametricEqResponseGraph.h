#pragma once

#include "ParametricEqModel.h"

#include <QPointF>
#include <QWidget>

#include <functional>

namespace entropy::host
{
class ParametricEqResponseGraph final : public QWidget
{
public:
    explicit ParametricEqResponseGraph(QWidget* parent = nullptr);
    void setModel(ParametricEqModel* model) noexcept;
    void setEditCallback(std::function<void()> callback);
    void setSelectionCallback(std::function<void(int)> callback);
    void armBandForWheel(int band) noexcept;
    QPointF nodePosition(int band) const noexcept;
    int hitTestNode(const QPointF& position) const noexcept;

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;

private:
    QRectF plotRect() const noexcept;
    double xToFrequency(double x) const noexcept;
    double yToGain(double y) const noexcept;
    ParametricEqModel* model_ = nullptr;
    int draggedBand_ = -1;
    int wheelArmedBand_ = -1;
    std::function<void()> editCallback_;
    std::function<void(int)> selectionCallback_;
};
} // namespace entropy::host
