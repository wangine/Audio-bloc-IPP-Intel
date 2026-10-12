#pragma once
#include <QFrame>
#include <QMouseEvent>
#include <QTimer>
#include <QPainter>
#include <QColor>
#include "R10/LiveUiRefresh.h"
#include <functional>
#include <cmath>
#include <cstdint>
namespace entropy::stage {
using entropy::host::liveUiRefreshAllowed;
class GraphNodeFrame : public QFrame
{
public:
    explicit GraphNodeFrame(QWidget* parent = nullptr) : QFrame(parent)
    {
        setProperty("clickable", true);
        setCursor(Qt::PointingHandCursor);
    }

    void setActivated(std::function<void()> callback)
    {
        callback_ = std::move(callback);
    }

protected:
    void mouseReleaseEvent(QMouseEvent* event) override
    {
        QFrame::mouseReleaseEvent(event);
        if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()) && callback_)
            callback_();
    }

private:
    std::function<void()> callback_;
};
class MatrixGraphNode final : public GraphNodeFrame
{
public:
    MatrixGraphNode(QString label, int inputCount, int outputCount,
                    QColor accent, QWidget* parent = nullptr)
        : GraphNodeFrame(parent), label_(std::move(label)), inputCount_(inputCount),
          outputCount_(outputCount), accent_(std::move(accent))
    {
        setProperty("graphNode", true);
        setFixedWidth(48);
        setMinimumHeight(500);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        setToolTip(QString("%1  •  %2 inputs / %3 outputs")
            .arg(label_).arg(inputCount_).arg(outputCount_));
        auto* pulseTimer = new QTimer(this);
        pulseTimer->setInterval(45);
        QObject::connect(pulseTimer, &QTimer::timeout, this, [this] {
            if (!liveUiRefreshAllowed(this)) return;
            pulsePhase_ += 0.085;
            if (pulsePhase_ > 6.283185307179586) pulsePhase_ -= 6.283185307179586;
            update();
        });
        pulseTimer->start();
    }

    int inputCount() const {return inputCount_;}
    int outputCount() const {return outputCount_;}
    uint32_t inputMask() const {return inputMask_;}
    uint32_t outputMask() const {return outputMask_;}
    double pulsePhase() const {return pulsePhase_;}
    void setPulsePhase(double phase) {pulsePhase_=phase;update();}
    void setLaneActivity(std::uint32_t inputMask, std::uint32_t outputMask,
                         std::uint32_t specialOutputMask, bool available)
    {
        inputMask_ = inputMask;
        outputMask_ = outputMask;
        specialOutputMask_ = specialOutputMask;
        available_ = available;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        GraphNodeFrame::paintEvent(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const auto drawLanes = [&](int count, std::uint32_t mask, bool input) {
            const double top = 24.0;
            const double bottom = height() - 24.0;
            for (int lane = 0; lane < count; ++lane)
            {
                const double y = count == 1 ? height() * 0.5
                    : top + (bottom - top) * lane / static_cast<double>(count - 1);
                const bool active = available_ && (mask & (1u << lane)) != 0;
                QColor color = active ? laneColor(lane, input) : QColor("#40515c");
                if (!input && active && (specialOutputMask_ & (1u << lane)) != 0)
                    color = QColor("#ffb34d");
                QPen pen(color, active ? 2.3 : 1.2, Qt::SolidLine, Qt::RoundCap);
                painter.setPen(pen);
                if (input) painter.drawLine(QPointF(0, y), QPointF(13, y));
                else painter.drawLine(QPointF(width() - 13, y), QPointF(width(), y));
                if (active)
                {
                    const double blend = 0.5 + 0.5 * std::sin(pulsePhase_);
                    QColor led(
                        qRound(35.0 + blend * 220.0),
                        qRound(185.0 - blend * 135.0),
                        qRound(255.0 - blend * 180.0));
                    led.setAlpha(qRound(145.0 + blend * 110.0));
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(led);
                    painter.drawEllipse(QPointF(input ? 13.0 : width() - 13.0, y),
                                        2.8 + blend * 1.2, 2.8 + blend * 1.2);
                }
            }
        };
        drawLanes(inputCount_, inputMask_, true);
        drawLanes(outputCount_, outputMask_, false);

        painter.save();
        painter.translate(width() * 0.5, height() * 0.5);
        painter.rotate(-90.0);
        QFont titleFont = font();
        titleFont.setBold(true);
        titleFont.setPointSizeF(10.5);
        painter.setFont(titleFont);
        painter.setPen(available_ ? accent_ : QColor("#60717b"));
        painter.drawText(QRectF(-height() * 0.22, -12.0, height() * 0.44, 24.0),
                         Qt::AlignCenter, label_);
        painter.restore();

        painter.setPen(Qt::NoPen);
        painter.setBrush(available_ ? QColor("#43e878") : QColor("#ff7070"));
        painter.drawEllipse(QPointF(width() * 0.5, height() - 11.0), 3.5, 3.5);
    }

private:
    QColor laneColor(int lane, bool) const {
        if(lane==0)return QColor("#20bfff");
        if(lane==1)return QColor("#46df70");
        if(label_=="MX OUT" && lane==2)return QColor("#ffb34d");
        return QColor("#aa70ff");
    }
    QString label_;
    int inputCount_ = 0;
    int outputCount_ = 0;
    QColor accent_;
    std::uint32_t inputMask_ = 0;
    std::uint32_t outputMask_ = 0;
    std::uint32_t specialOutputMask_ = 0;
    bool available_ = false;
    double pulsePhase_ = 0.0;
};
}
