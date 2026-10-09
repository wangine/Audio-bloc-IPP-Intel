#include "LiveUiRefresh.h"
#include "CrossoverPage.h"
#include "OutputStatusSemantics.h"
#include "DelayEditModel.h"
#include "Core/EngineTelemetry.h"

#include <entropy/crossover_abi.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
#include <QComboBox>
#include <map>
#include <QCheckBox>
#include <QCursor>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHideEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QTabWidget>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>

namespace entropy::host
{
namespace
{
constexpr const char* moduleId = "audio.entropy.crossover";

QString channelId(const char* base, int channel)
{
    if (channel < 0 || channel >= 16) return {};
    return QString::fromLatin1(base) + QString::number(channel + 1);
}

const std::array<QColor, 5> curveColours {
    QColor(0x48, 0xd6, 0xff), QColor(0xff, 0xc8, 0x57),
    QColor(0x7e, 0xe7, 0x87), QColor(0xff, 0x7b, 0x72),
    QColor(0xd2, 0xa8, 0xff)
};

class ThinMeterBank final : public QWidget
{
public:
    struct Meter { QString label; QColor colour; double linear = 0.0; };
    explicit ThinMeterBank(QWidget* parent = nullptr) : QWidget(parent)
    {
        setFixedWidth(72);
        setMinimumHeight(120);
    }
    void setMeters(std::vector<Meter> meters)
    {
        meters_ = std::move(meters);
        update();
    }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.fillRect(rect(), QColor(0x0c, 0x11, 0x17));
        if (meters_.empty()) return;
        const int gap = 3;
        const int laneWidth = std::max(5, (width() - gap * (int(meters_.size()) + 1))
                                         / int(meters_.size()));
        for (int index = 0; index < int(meters_.size()); ++index)
        {
            const auto& meter = meters_[size_t(index)];
            const int x = gap + index * (laneWidth + gap);
            QRect bar(x, 18, laneWidth, std::max(1, height() - 36));
            painter.fillRect(bar, QColor(0x17, 0x1d, 0x24));
            const double db = meter.linear > 1.0e-9
                ? std::max(-72.0, 20.0 * std::log10(meter.linear)) : -72.0;
            const int filled = qRound((db + 72.0) / 72.0 * bar.height());
            QRect level = bar;
            level.setTop(bar.bottom() - filled + 1);
            painter.fillRect(level, meter.colour);
            painter.setPen(QColor(0xa8, 0xb2, 0xbc));
            painter.drawRect(bar.adjusted(0, 0, -1, -1));
            painter.setFont(QFont(painter.font().family(), 8));
            painter.drawText(QRect(x - 2, 0, laneWidth + 4, 16),
                             Qt::AlignCenter, meter.label);
        }
    }
private:
    std::vector<Meter> meters_;
};

class CrossoverResponseGraph final : public QWidget
{
public:
    struct Curve
    {
        int channel = -1;
        QString label;
        QColor colour;
        std::array<float, crossoverResponsePointCount> decibels {};
    };
    std::function<void(int, bool, bool)> onCutoffGesture;
    std::function<void(int, bool, double)> onCutoffDragged;

    explicit CrossoverResponseGraph(QWidget* parent = nullptr) : QWidget(parent)
    {
        setMinimumHeight(220);
        setMouseTracking(true);
    }

    void setData(std::vector<Curve> curves, int selectedChannel,
                 double minimumHz, double maximumHz,
                 double hpHz, double lpHz,
                 const float* spectrumHz, const float* spectrumDb,
                 std::uint32_t spectrumPoints, bool telemetryAvailable)
    {
        curves_ = std::move(curves);
        selectedChannel_ = selectedChannel;
        minimumHz_ = std::max(1.0, minimumHz);
        maximumHz_ = std::max(minimumHz_ + 1.0, maximumHz);
        hpHz_ = hpHz;
        lpHz_ = lpHz;
        telemetryAvailable_ = telemetryAvailable;
        spectrum_.clear();
        for (std::uint32_t i = 0; i < spectrumPoints; ++i)
            spectrum_.push_back({spectrumHz[i], spectrumDb[i]});
        update();
    }

protected:
    QRectF plotBounds() const
    {
        auto plot = QRectF(rect()).adjusted(14.0, 12.0, -14.0, -12.0);
        plot.adjust(0.0, 24.0, 0.0, -18.0);
        return plot;
    }
    double frequencyAt(double x) const
    {
        const auto plot = plotBounds();
        const double unit = std::clamp((x - plot.left()) / plot.width(), 0.0, 1.0);
        return std::exp(std::log(minimumHz_) +
                        unit * (std::log(maximumHz_) - std::log(minimumHz_)));
    }
    double xFor(double frequency) const
    {
        const auto plot = plotBounds();
        const double unit = (std::log(std::clamp(frequency, minimumHz_, maximumHz_))
                           - std::log(minimumHz_))
                          / (std::log(maximumHz_) - std::log(minimumHz_));
        return plot.left() + unit * plot.width();
    }
    double yFor(double db) const
    {
        const auto plot = plotBounds();
        return plot.bottom() - std::clamp((db + 72.0) / 84.0, 0.0, 1.0) * plot.height();
    }
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0x0c, 0x11, 0x17));
        painter.drawRoundedRect(QRectF(rect()), 8.0, 8.0);
        const auto plot = plotBounds();
        painter.setPen(QPen(QColor(0x2c, 0x35, 0x40), 1));
        for (int division = 0; division <= 8; ++division)
        {
            const double x = plot.left() + plot.width() * division / 8.0;
            painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        }
        for (int division = 0; division <= 5; ++division)
        {
            const double y = plot.top() + plot.height() * division / 5.0;
            painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        }
        painter.setPen(QColor(255, 255, 255, 166));
        painter.drawText(QRectF(plot.left(), plot.bottom() + 2.0, 70.0, 16.0),
                         Qt::AlignLeft | Qt::AlignTop,
                         QString::number(qRound(minimumHz_)) + " Hz");
        painter.drawText(QRectF(plot.right() - 70.0, plot.bottom() + 2.0, 70.0, 16.0),
                         Qt::AlignRight | Qt::AlignTop,
                         QString::number(qRound(maximumHz_ / 1000.0)) + " kHz");
        if (!telemetryAvailable_)
        {
            painter.setPen(QColor(0xa8, 0xb2, 0xbc));
            painter.drawText(plot, Qt::AlignCenter, "TELEMETRY UNAVAILABLE");
            return;
        }
        if (!spectrum_.empty())
        {
            QPainterPath spectrumPath;
            bool first = true;
            for (const auto& point : spectrum_)
            {
                if (point.first < minimumHz_ || point.first > maximumHz_) continue;
                const QPointF p(xFor(point.first), yFor(point.second));
                first ? spectrumPath.moveTo(p) : spectrumPath.lineTo(p);
                first = false;
            }
            painter.save();
            painter.setClipRect(plot);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(225, 238, 248, 92), 1.0));
            painter.drawPath(spectrumPath);
            painter.restore();
        }
        int legend = 0;
        for (const auto& curve : curves_)
        {
            QPainterPath path;
            for (std::uint32_t point = 0; point < crossoverResponsePointCount; ++point)
            {
                const double unit = point / double(crossoverResponsePointCount - 1u);
                const double frequency = std::exp(std::log(minimumHz_) +
                    unit * (std::log(maximumHz_) - std::log(minimumHz_)));
                const QPointF p(xFor(frequency), yFor(curve.decibels[point]));
                point == 0 ? path.moveTo(p) : path.lineTo(p);
            }
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(curve.colour, curve.channel == selectedChannel_ ? 3.2 : 1.8));
            painter.drawPath(path);
            painter.drawText(QRectF(plot.left() + legend * 132, plot.top() - 23, 125, 18),
                             Qt::AlignLeft | Qt::AlignVCenter, curve.label);
            ++legend;
        }
        if (selectedChannel_ >= 0)
        {
            painter.setPen(QPen(QColor(0x48, 0xd6, 0xff, 150), 1, Qt::DashLine));
            painter.drawLine(QPointF(xFor(hpHz_), plot.top()), QPointF(xFor(hpHz_), plot.bottom()));
            painter.setPen(QPen(QColor(0xff, 0xc8, 0x57, 150), 1, Qt::DashLine));
            painter.drawLine(QPointF(xFor(lpHz_), plot.top()), QPointF(xFor(lpHz_), plot.bottom()));
        }
        if (hovering_ && plot.contains(hoverPosition_))
        {
            const double frequency = frequencyAt(hoverPosition_.x());
            painter.setPen(QPen(QColor(220, 232, 240, 105), 1, Qt::DotLine));
            painter.drawLine(QPointF(hoverPosition_.x(), plot.top()),
                             QPointF(hoverPosition_.x(), plot.bottom()));
            const QString readout = frequency >= 1000.0
                ? QString("%1 kHz").arg(frequency / 1000.0, 0, 'f', 2)
                : QString("%1 Hz").arg(frequency, 0, 'f', 1);
            const QFontMetrics metrics(painter.font());
            const QSize textSize = metrics.size(Qt::TextSingleLine, readout) + QSize(12, 6);
            QRectF badge(hoverPosition_.x() + 8.0, plot.top() + 7.0,
                         textSize.width(), textSize.height());
            if (badge.right() > plot.right()) badge.moveRight(hoverPosition_.x() - 8.0);
            painter.setPen(QPen(QColor(0x65, 0xc7, 0xd9), 1));
            painter.setBrush(QColor(0x0c, 0x11, 0x17, 232));
            painter.drawRoundedRect(badge, 3.0, 3.0);
            painter.setPen(QColor(0xe8, 0xf6, 0xff));
            painter.drawText(badge, Qt::AlignCenter, readout);
        }
    }
    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() != Qt::LeftButton || selectedChannel_ < 0 ||
            !plotBounds().contains(event->position())) return;
        const double hpDistance = std::abs(event->position().x() - xFor(hpHz_));
        const double lpDistance = std::abs(event->position().x() - xFor(lpHz_));
        if (std::min(hpDistance, lpDistance) > 9.0) return;
        dragging_ = true;
        draggingHighPass_ = hpDistance <= lpDistance;
        if (onCutoffGesture) onCutoffGesture(selectedChannel_, draggingHighPass_, true);
    }
    void mouseMoveEvent(QMouseEvent* event) override
    {
        hovering_ = plotBounds().contains(event->position());
        hoverPosition_ = event->position();
        if (!dragging_ || selectedChannel_ < 0)
        {
            update();
            return;
        }
        double frequency = frequencyAt(event->position().x());
        if (draggingHighPass_) frequency = std::min(frequency, lpHz_);
        else frequency = std::max(frequency, hpHz_);
        if (draggingHighPass_) hpHz_ = frequency; else lpHz_ = frequency;
        if (onCutoffDragged) onCutoffDragged(selectedChannel_, draggingHighPass_, frequency);
        update();
    }
    void leaveEvent(QEvent* event) override
    {
        hovering_ = false;
        update();
        QWidget::leaveEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent*) override
    {
        if (!dragging_) return;
        dragging_ = false;
        if (onCutoffGesture) onCutoffGesture(selectedChannel_, draggingHighPass_, false);
    }
    void hideEvent(QHideEvent* event) override
    {
        if (dragging_ && onCutoffGesture)
            onCutoffGesture(selectedChannel_, draggingHighPass_, false);
        dragging_ = false;
        QWidget::hideEvent(event);
    }
private:
    std::vector<Curve> curves_;
    std::vector<std::pair<float, float>> spectrum_;
    int selectedChannel_ = -1;
    double minimumHz_ = 20.0;
    double maximumHz_ = 60000.0;
    double hpHz_ = 80.0;
    double lpHz_ = 20000.0;
    bool telemetryAvailable_ = false;
    bool dragging_ = false;
    bool draggingHighPass_ = true;
    bool hovering_ = false;
    QPointF hoverPosition_;
};

class CrossoverView final : public QWidget
{
public:
    explicit CrossoverView(QWidget* parent, bool openBandwidthProtector) : QWidget(parent)
    {
        setMinimumSize(780, 500);
        setStyleSheet(R"(
          QWidget#crossoverRoot { background:#171c22; color:#dfe7ed; }
          QTabWidget::pane { border:0; background:#171c22; }
          QTabBar::tab { background:#252c34; color:#aeb6bf; min-width:112px;
                         height:24px; padding:0 14px; border:0; }
          QTabBar::tab:selected { background:#202833; color:white; border-bottom:2px solid #48d6ff; }
          QTabBar::tab:hover { background:#2c3540; color:white; }
          QComboBox { background:#202833; border:1px solid #39434d; min-height:20px; padding:0 6px; }
          QCheckBox { spacing:7px; }
          QCheckBox::indicator { width:15px; height:15px; }
          QPushButton { min-height:22px; padding:2px 9px; background:#202833;
                        border:1px solid #39434d; color:#dfe7ed; }
          QPushButton:hover { border-color:#48d6ff; color:white; }
          QPushButton:disabled { color:#626a73; background:#111417; border-color:#303945; }
          QSlider::groove:horizontal { height:5px; background:#0c1117; border:1px solid #303945; }
          QSlider::sub-page:horizontal { background:#48d6ff; }
          QSlider::handle:horizontal { width:12px; margin:-5px 0; background:#e8f6ff;
                                       border:1px solid #48d6ff; border-radius:6px; }
          QProgressBar { background:#0c1117; border:1px solid #303945; }
        )");
        setObjectName("crossoverRoot");
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(10, 10, 10, 10);
        root->setSpacing(4);
        auto* top = new QHBoxLayout;
        top->setContentsMargins(0, 0, 0, 0);
        auto* title = new QLabel("ENTROPY / CROSSOVER");
        title->setStyleSheet("font-size:21px;font-weight:700;");
        title->setFixedHeight(30);
        runtime_ = new QLabel;
        runtime_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        top->addWidget(title);
        top->addStretch();
        top->addWidget(runtime_);
        root->addLayout(top);
        tabs_ = new QTabWidget;
        tabs_->setObjectName(QStringLiteral("crossoverTabs"));
        root->addWidget(tabs_, 1);
        buildRouting();
        buildFilterPage("CROSSOVER L/R", true, false);
        buildFilterPage("CENTER", false, false);
        buildFilterPage("BASS & SCOOP", false, true);
        buildBandwidthProtector();
        buildCinema();
        buildModule();
        if (openBandwidthProtector)
            tabs_->setCurrentIndex(4);
        auto* timer = new QTimer(this);
        timer->setInterval(50);
        connect(timer, &QTimer::timeout, this, [this] { if (liveUiRefreshAllowed(this)) refresh(); });
        timer->start();
    }

private:
    struct ComboBinding
    {
        QComboBox* widget;
        std::function<QString()> id;
        QVector<int> semanticValues;
    };
    struct ToggleBinding { QCheckBox* widget; std::function<QString()> id; };
    struct SliderBinding
    {
        QSlider* widget;
        QLabel* label;
        QString caption;
        QString suffix;
        double scale;
        int decimals;
        std::function<QString()> id;
    };
    struct FilterPageBinding
    {
        QComboBox* selector;
        QComboBox* side;
        QSlider* lowPassSlider;
        CrossoverResponseGraph* graph;
        ThinMeterBank* meters;
        bool lr;
        bool bass;
    };

    bool selectedBassLaneIsScoop() const
    {
        if (bassLaneSelector_ == nullptr) return false;
        const int role = bassLaneSelector_->currentData().toInt();
        return role == 17 || role == 19;
    }

    static QLabel* label(const QString& text)
    {
        auto* value = new QLabel(text);
        return value;
    }

    static void styleRoutingSlider(QSlider* control, bool delayControl)
    {
        const QString accent = delayControl ? QStringLiteral("#ffc400")
                                            : QStringLiteral("#48d6ff");
        const QString thumb = delayControl ? QStringLiteral("#ffd223")
                                           : QStringLiteral("#bfeeff");
        const QString background = delayControl ? QStringLiteral("#302600")
                                                : QStringLiteral("#0c1117");
        control->setStyleSheet(QString(
            "QSlider::groove:horizontal{height:3px;background:%1;border:0;}"
            "QSlider::sub-page:horizontal{height:3px;background:%2;border:0;}"
            "QSlider::handle:horizontal{width:8px;margin:-4px 0;background:%3;"
            "border:1px solid %2;border-radius:4px;}")
            .arg(background, accent, thumb));
    }

    static void styleChannelDelaySlider(QSlider* control, const QColor& colour)
    {
        const auto accent = colour.name();
        const auto thumb = colour.lighter(135).name();
        const auto background = colour.darker(500).name();
        control->setStyleSheet(QString(
            "QSlider::groove:horizontal{height:3px;background:%1;border:0;}"
            "QSlider::sub-page:horizontal{height:3px;background:%2;border:0;}"
            "QSlider::handle:horizontal{width:8px;margin:-4px 0;background:%3;"
            "border:1px solid %2;border-radius:4px;}")
            .arg(background, accent, thumb));
    }

    QComboBox* combo(QGridLayout* layout, int row, const QString& caption,
                     const QStringList& values, std::function<QString()> id,
                     QVector<int> semanticValues = {}, int column = 0)
    {
        auto* box = new QComboBox;
        box->addItems(values);
        layout->addWidget(label(caption), row, column);
        layout->addWidget(box, row, column + 1);
        comboBindings_.push_back({box, id, semanticValues});
        connect(box, &QComboBox::currentIndexChanged, this,
            [id, box, semanticValues](int index) {
                if (box->signalsBlocked()) return;
                const int value = semanticValues.isEmpty() || index < 0 ||
                                  index >= semanticValues.size()
                    ? index : semanticValues[index];
                send(id(), value);
            });
        return box;
    }

    QCheckBox* toggle(QGridLayout* layout, int row, const QString& caption,
                        std::function<QString()> id, int column = 0)
    {
        auto* button = new QCheckBox(caption);
        button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
        layout->addWidget(button, row, column);
        toggleBindings_.push_back({button, id});
        connect(button, &QCheckBox::toggled, this,
            [id, button](bool value) { if (!button->signalsBlocked()) send(id(), value ? 1.0 : 0.0); });
        return button;
    }

    QSlider* slider(QGridLayout* layout, int row, const QString& caption,
                    double minimum, double maximum, double value, double scale,
                    int decimals, const QString& suffix, std::function<QString()> id,
                    int column = 0, int columnSpan = 1)
    {
        auto* text = label(caption);
        auto* control = new QSlider(Qt::Horizontal);
        control->setRange(qRound(minimum * scale), qRound(maximum * scale));
        control->setValue(qRound(value * scale));
        layout->addWidget(text, row, column);
        layout->addWidget(control, row, column + 1, 1, columnSpan);
        sliderBindings_.push_back({control, text, caption, suffix, scale, decimals, id});
        connect(control, &QSlider::valueChanged, this,
            [id, control, scale](int raw) {
                if (!control->signalsBlocked()) send(id(), raw / scale);
            });
        connect(control, &QSlider::sliderPressed, this,
            [id] { sendGesture(id(), true); });
        connect(control, &QSlider::sliderReleased, this,
            [id] { sendGesture(id(), false); });
        return control;
    }

    static void send(const QString& id, double value)
    {
        if (id.isEmpty()) return;
        const auto bytes = id.toUtf8();
        if(!writeEngineParameterIntent(moduleId, bytes.constData(), value))
            QToolTip::showText(QCursor::pos(), "Crossover change refused: control queue full or engine unavailable. Retry the change.", nullptr, {}, 5000);
    }

    static void sendGesture(const QString& id, bool begin)
    {
        if (id.isEmpty()) return;
        const auto bytes = id.toUtf8();
        if(!writeEngineParameterGesture(moduleId, bytes.constData(), begin))
            QToolTip::showText(QCursor::pos(), "Crossover gesture refused: control queue full or engine unavailable. Repeat the gesture after the queue drains.", nullptr, {}, 5000);
    }

    QString selectedDelayId() const
    {
        return channelId(lastTelemetry_.crossoverCinemaProgram
            ? "cinemaDelayMs" : "musicDelayMs", selectedOutput_->currentIndex());
    }

    auto delaySink()
    {
        return [this](const std::string& identifier, int kind, double value) {
            const QString id = QString::fromStdString(identifier);
            const bool ok = kind == 1
                ? writeEngineParameterIntent(moduleId, identifier.c_str(), value)
                : writeEngineParameterGesture(moduleId, identifier.c_str(), kind == 0);
            if (kind == 1 && ok) pendingDelays_[id] = {value, false, lastTelemetry_.crossoverControlAppliedSequence};
            if (!ok)
            {
                pendingDelays_[id] = {value, true, lastTelemetry_.crossoverControlAppliedSequence};
                QToolTip::showText(QCursor::pos(),
                    QString("Delay %1 refused for %2: queue full or engine unavailable. Retry this output.")
                        .arg(kind == 0 ? "BEGIN" : kind == 1 ? "SET" : "END", id),
                    this, {}, 5000);
            }
            return ok;
        };
    }

    void beginDelayEdit(ui::DelayEditModel& edit)
    {
        if (edit.active()) return;
        if (fineDelayEdit_.active() || coarseDelayEdit_.active()) return;
        const auto id = selectedDelayId();
        const auto pending = pendingDelays_.find(id);
        const double initial = pending != pendingDelays_.end() && !pending->second.failed
            ? pending->second.value : parameter(lastTelemetry_, id, 0.0);
        edit.begin(id.toStdString(), initial, delaySink());
    }

    void refreshDelay(const EngineTelemetrySnapshot& telemetry)
    {
        const ui::DelayEditModel* active = fineDelayEdit_.active() ? &fineDelayEdit_
            : coarseDelayEdit_.active() ? &coarseDelayEdit_ : nullptr;
        const QString id = active ? QString::fromStdString(active->id()) : selectedDelayId();
        const bool cinema = id.startsWith("cinema");
        const int channel = id.mid(cinema ? 13 : 12).toInt() - 1;
        const double applied = parameter(telemetry, id, 0.0);
        auto pending = pendingDelays_.find(id);
        const bool parameterAcknowledged =
            telemetry.crossoverCommandState == ENTROPY_CROSSOVER_COMMAND_APPLIED
            && telemetry.crossoverCommandAppliedSequence == telemetry.crossoverCommandRequestedSequence;
        // The merged ledger reports a confirmed control-only END as ACCEPTED,
        // retaining the immediately preceding SET's native adoption sequence.
        const bool finalSetAcknowledgedBeforeEnd =
            telemetry.crossoverCommandState == ENTROPY_CROSSOVER_COMMAND_ACCEPTED
            && telemetry.crossoverCommandKind == ENTROPY_CROSSOVER_COMMAND_GESTURE_END
            && telemetry.crossoverCommandAcceptedSequence == telemetry.crossoverCommandRequestedSequence
            && telemetry.crossoverCommandAppliedSequence != 0
            && telemetry.crossoverCommandAppliedSequence + 1 == telemetry.crossoverCommandRequestedSequence;
        // Existing APVTS float normalization spans 1600 ms.
        if (pending != pendingDelays_.end() && !pending->second.failed
            && (parameterAcknowledged || finalSetAcknowledgedBeforeEnd)
            && telemetry.crossoverCommandRequestedSequence != 0
            && telemetry.crossoverControlAppliedSequence > pending->second.afterSequence
            && id == QString::fromLatin1(telemetry.crossoverLastParameter)
            && std::abs(telemetry.crossoverLastValue - pending->second.value) <= .00013
            && std::abs(applied - pending->second.value) <= .00013)
        {
            pendingDelays_.erase(pending);
            pending = pendingDelays_.end();
        }
        const double shown = active ? active->value()
            : pending != pendingDelays_.end() ? pending->second.value : applied;
        if (!coarseDelaySlider_->isSliderDown())
        {
            const QSignalBlocker blocker(coarseDelaySlider_);
            coarseDelaySlider_->setValue(qRound(shown * 1000));
        }
        coarseDelayLabel_->setText(QString("RELATIVE DELAY / COARSE   %1 ms").arg(shown,0,'f',3));
        if (!fineDelayEdit_.active())
            fineDelayLabel_->setText(QString("FINE ADJUST   %1 ms").arg(shown,0,'f',3));
        const double effective = ui::DelayEditModel::effective(applied);
        const int role = qRound(parameter(telemetry,channelId(cinema ? "cinemaRole" : "source",channel),0));
        const QStringList roles {"OFF", "L WAY 1", "L WAY 2", "L WAY 3", "L WAY 4", "L WAY 5",
            "R WAY 1", "R WAY 2", "R WAY 3", "R WAY 4", "R WAY 5", "CENTER WAY 1",
            "CENTER WAY 2", "CENTER WAY 3", "CENTER WAY 4", "CENTER WAY 5", "SUB 1", "SCOOP 1",
            "SUB 2", "SCOOP 2", "CINEMA SL", "CINEMA SR", "CINEMA BL", "CINEMA BR", "CINEMA TSL", "CINEMA TSR"};
        const QString state = active ? "EDITING" : pending != pendingDelays_.end()
            ? pending->second.failed ? "FAILED — retry" : "REQUESTED — application unconfirmed" : "PARAMETER";
        QString clamp;
        if (applied < 0) clamp = " | CLAMPED AT 0 ms: negative relative changes here do not shift output";
        else if (applied > 1600) clamp = " | CLAMPED AT 1600 ms";
        delayStatus_->setText(QString("%1 | CH %2 | %3 | %4\nChannel delay %5 ms = effective target %6 ms%7")
            .arg(cinema ? "CINEMA" : "MUSIC").arg(channel+1)
            .arg(roles.value(role,"OFF"),state).arg(applied,0,'f',6)
            .arg(effective,0,'f',6).arg(clamp));
        delayStatus_->setToolTip(QString("Parameter: %1. Effective target clamps channel delay to [0,1600] ms. Parameter readback alone does not prove audio adoption. Existing FIR latency compensation and smoothing remain active.").arg(id));
        const int fractionalMode=qRound(parameter(telemetry,channelId("fractionalDelayMode",channel),0));
        const int requestedOrder=fractionalMode==3?5:fractionalMode==2?3:fractionalMode==1?1:0;
        const int actualOrder=int(telemetry.crossoverFractionalDelayOrder[channel]);
        QString delayMode=fractionalMode
            ? QString("THIRAN requested %1 / effective order %2%3").arg(requestedOrder).arg(actualOrder)
                .arg(telemetry.crossoverFractionalDelayTransition[channel]?" | 40 ms TRANSITION":"")
            : QStringLiteral("LINEAR reference");
        if(fractionalMode && actualOrder<requestedOrder)
            delayMode += actualOrder?" | short delay: lower causal order":" | integer bypass or inactive output";
        delayStatus_->setText(delayStatus_->text()+"\n"+delayMode);
        const bool available = telemetry.crossoverControlAvailable != 0;
        fineDelaySlider_->setEnabled(available && (!active || active == &fineDelayEdit_));
        coarseDelaySlider_->setEnabled(available && (!active || active == &coarseDelayEdit_));
    }

    void applyLeftToRight()
    {
        if (copyLeftToRight_ == nullptr || !lastTelemetryValid_ ||
            lastTelemetry_.crossoverControlAvailable == 0)
            return;
        const bool queued = writeEngineCrossoverApplyLeftToRight(
            lastTelemetry_.crossoverCinemaProgram != 0);
        copyLeftToRight_->setText(queued ? "L → R REQUESTED" : "L → R FAILED");
        copyLeftToRight_->setEnabled(false);
        QTimer::singleShot(900, this, [this] {
            if (copyLeftToRight_ == nullptr) return;
            copyLeftToRight_->setText("APPLY ALL L → R");
            copyLeftToRight_->setEnabled(
                lastTelemetryValid_ && lastTelemetry_.crossoverControlAvailable != 0);
        });
    }

    static double parameter(const EngineTelemetrySnapshot& telemetry,
                            const QString& identifier, double fallback)
    {
        const auto wanted = identifier.toUtf8();
        for (std::uint32_t index = 0; index < telemetry.crossoverParameterCount; ++index)
            if (std::strcmp(telemetry.crossoverParameters[index].identifier, wanted.constData()) == 0)
                return telemetry.crossoverParameters[index].value;
        return fallback;
    }

    static int channelForRole(const EngineTelemetrySnapshot& telemetry, int role)
    {
        const char* rolePrefix = telemetry.crossoverCinemaProgram ? "cinemaRole" : "source";
        for (int channel = 0; channel < 16; ++channel)
            if (qRound(parameter(telemetry, channelId(rolePrefix, channel), 0.0)) == role)
                return channel;
        return -1;
    }

    int selectedPhysicalChannel(QComboBox* selector, QComboBox* side,
                                bool lr, bool bass) const
    {
        if (!lastTelemetryValid_) return -1;
        int role = 0;
        if (lr)
            role = (side != nullptr && side->currentIndex() == 1 ? 5 : 0)
                 + selector->currentData().toInt() + 1;
        else if (bass)
            role = selector->currentData().toInt();
        else
            role = 11 + selector->currentData().toInt();
        return channelForRole(lastTelemetry_, role);
    }

    static void setSelectorItems(QComboBox* selector,
                                 const std::vector<std::pair<QString, int>>& items)
    {
        bool identical = selector->count() == static_cast<int>(items.size());
        for (int index = 0; identical && index < selector->count(); ++index)
            identical = selector->itemText(index) == items[size_t(index)].first &&
                        selector->itemData(index).toInt() == items[size_t(index)].second;
        if (identical) return;
        const int previous = selector->currentData().toInt();
        const QSignalBlocker blocker(selector);
        selector->clear();
        for (const auto& item : items)
            selector->addItem(item.first, item.second);
        const int restored = selector->findData(previous);
        selector->setCurrentIndex(restored >= 0 ? restored : 0);
    }

    QWidget* page(const QString& name, QGridLayout*& layout)
    {
        auto* widget = new QWidget;
        layout = new QGridLayout(widget);
        layout->setContentsMargins(14, 14, 14, 14);
        layout->setColumnStretch(1, 1);
        tabs_->addTab(widget, name);
        return widget;
    }

    void buildRouting()
    {
        QGridLayout* grid{}; page("ROUTING", grid);
        grid->setVerticalSpacing(8);
        auto* heading = label("ROUTING / SPEAKER FORMAT");
        heading->setStyleSheet("font-size:18px;font-weight:700;");
        grid->addWidget(heading, 0, 0, 1, 4);
        auto* topology = new QWidget;
        auto* topologyGrid = new QGridLayout(topology);
        topologyGrid->setContentsMargins(0, 0, 0, 0);
        topologyGrid->setHorizontalSpacing(12);
        combo(topologyGrid, 0, "L/R SPEAKER", {"1 WAY","2 WAYS","3 WAYS","4 WAYS","5 WAYS"},
              []{return "lrWays";}, {1,2,3,4,5}, 0);
        combo(topologyGrid, 0, "CENTER SPEAKER", {"1 WAY","2 WAYS"},
              []{return "centerWays";}, {1,2}, 2);
        combo(topologyGrid, 0, "SUB", {"1 MONO","2 STEREO"},
              []{return "subSpeakers";}, {1,2}, 4);
        combo(topologyGrid, 0, "SCOOP", {"1 MONO","2 STEREO"},
              []{return "scoopSpeakers";}, {1,2}, 6);
        grid->addWidget(topology, 1, 0, 1, 4);
        auto* hint = label("MUSIC and CINEMA keep separate role maps, output gains and 16 channel delays. Switch program in CINEMA; ROUTING follows the active scene.");
        hint->setStyleSheet("color:#aeb6bf;");
        grid->addWidget(hint, 2, 0, 1, 4);
        auto* tiles = new QWidget;
        auto* tileGrid = new QGridLayout(tiles);
        tileGrid->setContentsMargins(0, 4, 0, 4);
        tileGrid->setSpacing(4);
        for (int channel = 0; channel < 16; ++channel)
        {
            auto* tile = new QPushButton(QString("CH%1   OFF").arg(channel + 1));
            tile->setCheckable(true);
            tile->setMinimumHeight(27);
            tileGrid->addWidget(tile, channel / 4, channel % 4);
            routingTiles_[size_t(channel)] = tile;
            connect(tile, &QPushButton::clicked, this,
                    [this, channel] { selectedOutput_->setCurrentIndex(channel); });
        }
        grid->addWidget(tiles, 3, 0, 1, 4);
        selectedOutput_ = new QComboBox(this);
        selectedOutput_->addItems(channelNames());
        selectedOutput_->setObjectName("selectedDelayOutput");
        selectedOutput_->hide();
        const QStringList roles{"OFF","L WAY 1","L WAY 2","L WAY 3","L WAY 4","L WAY 5",
            "R WAY 1","R WAY 2","R WAY 3","R WAY 4","R WAY 5","CENTER WAY 1","CENTER WAY 2",
            "SUB 1","SCOOP 1","SUB 2","SCOOP 2","CINEMA SL","CINEMA SR","CINEMA BL","CINEMA BR","CINEMA TSL","CINEMA TSR"};
        combo(grid, 4, "ASSIGNED ROLE", roles,
              [this]{return channelId(lastTelemetry_.crossoverCinemaProgram
                    ? "cinemaRole" : "source", selectedOutput_->currentIndex());},
              {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
               16, 17, 18, 19, 20, 21, 22, 23, 24, 25});
        auto* gain = slider(grid, 5, "GAIN", -60, 12, 0, 10, 1, " dB", [this]{
            return channelId(lastTelemetry_.crossoverCinemaProgram
                ? "cinemaGainDb" : "musicGainDb", selectedOutput_->currentIndex());});
        styleRoutingSlider(gain, false);

        auto* fineRow = new QWidget;
        auto* fineLayout = new QHBoxLayout(fineRow);
        fineLayout->setContentsMargins(0, 0, 0, 0);
        fineLayout->setSpacing(10);
        fineDelayLabel_ = label("FINE ADJUST");
        fineDelayLabel_->setMinimumWidth(150);
        fineDelaySlider_ = new QSlider(Qt::Horizontal);
        fineDelaySlider_->setObjectName("fineDelaySlider");
        fineDelaySlider_->setRange(-500, 500);
        fineDelaySlider_->setValue(0);
        fineDelaySlider_->setToolTip("Geared adjustment of RELATIVE DELAY for the selected logical output.");
        styleRoutingSlider(fineDelaySlider_, true);
        fineRatio_ = new QComboBox;
        fineRatio_->addItems({"1/10", "1/100", "1/1000"});
        fineRatio_->setCurrentIndex(1);
        fineRatio_->setFixedWidth(90);
        fineLayout->addWidget(fineDelayLabel_);
        fineLayout->addWidget(fineDelaySlider_, 1);
        fineLayout->addWidget(label("RATIO"));
        fineLayout->addWidget(fineRatio_);
        grid->addWidget(fineRow, 6, 0, 1, 4);

        connect(fineDelaySlider_, &QSlider::sliderPressed, this, [this] {
            beginDelayEdit(fineDelayEdit_);
            fineDelayLastRaw_ = fineDelaySlider_->value();
        });
        connect(fineDelaySlider_, &QSlider::valueChanged, this, [this](int raw) {
            if (!fineDelaySlider_->isSliderDown() || !fineDelayEdit_.active()) return;
            const int rawDelta = raw - fineDelayLastRaw_;
            fineDelayLastRaw_ = raw;
            const std::array<double, 3> ratios {0.1, 0.01, 0.001};
            const double ratio = ratios[size_t(std::clamp(fineRatio_->currentIndex(), 0, 2))];
            fineDelayEdit_.move(rawDelta, ratio, delaySink());
            fineDelayLabel_->setText(QString("FINE ADJUST   %1 ms").arg(fineDelayEdit_.value(), 0, 'f', 3));
            if (raw >= 499)
            {
                const QSignalBlocker blocker(fineDelaySlider_);
                fineDelaySlider_->setValue(-499);
                fineDelayLastRaw_ = -499;
                QCursor::setPos(fineDelaySlider_->mapToGlobal(
                    QPoint(2, fineDelaySlider_->height() / 2)));
            }
            else if (raw <= -499)
            {
                const QSignalBlocker blocker(fineDelaySlider_);
                fineDelaySlider_->setValue(499);
                fineDelayLastRaw_ = 499;
                QCursor::setPos(fineDelaySlider_->mapToGlobal(
                    QPoint(fineDelaySlider_->width() - 3, fineDelaySlider_->height() / 2)));
            }
        });
        connect(fineDelaySlider_, &QSlider::sliderReleased, this, [this] {
            fineDelayEdit_.finish(delaySink());
            const QSignalBlocker blocker(fineDelaySlider_);
            fineDelaySlider_->setValue(0);
        });

        coarseDelayLabel_ = label("RELATIVE DELAY / COARSE");
        coarseDelaySlider_ = new QSlider(Qt::Horizontal);
        coarseDelaySlider_->setObjectName("coarseDelaySlider");
        coarseDelaySlider_->setRange(-800000, 800000);
        grid->addWidget(coarseDelayLabel_, 7, 0);
        grid->addWidget(coarseDelaySlider_, 7, 1);
        styleRoutingSlider(coarseDelaySlider_, true);
        connect(coarseDelaySlider_, &QSlider::sliderPressed, this,
            [this] { beginDelayEdit(coarseDelayEdit_); });
        connect(coarseDelaySlider_, &QSlider::valueChanged, this, [this](int raw) {
            if (coarseDelaySlider_->signalsBlocked()) return;
            const bool discrete = !coarseDelaySlider_->isSliderDown();
            if (!coarseDelayEdit_.active()) beginDelayEdit(coarseDelayEdit_);
            coarseDelayEdit_.set(raw / 1000.0, delaySink());
            if (discrete) coarseDelayEdit_.finish(delaySink());
        });
        connect(coarseDelaySlider_, &QSlider::sliderReleased, this,
            [this] { coarseDelayEdit_.finish(delaySink()); });

        delayStatus_ = label("Waiting for applied delay");
        delayStatus_->setObjectName("relativeDelayStatus");
        delayStatus_->setWordWrap(true);
        grid->addWidget(delayStatus_, 9, 0, 1, 4);
        auto* actions = new QWidget;
        auto* actionRow = new QHBoxLayout(actions);
        actionRow->setContentsMargins(0, 0, 0, 0);
        auto* mute = new QCheckBox("MUTE");
        auto* solo = new QCheckBox("SOLO");
        auto* polarity = new QCheckBox("INVERT POLARITY");
        for (auto* button : {mute, solo, polarity}) actionRow->addWidget(button);
        toggleBindings_.push_back({mute, [this]{return channelId("mute", selectedOutput_->currentIndex());}});
        toggleBindings_.push_back({solo, [this]{return channelId("solo", selectedOutput_->currentIndex());}});
        toggleBindings_.push_back({polarity, [this]{return channelId("polarity", selectedOutput_->currentIndex());}});
        connect(mute, &QCheckBox::toggled, this, [this, mute](bool v){if(!mute->signalsBlocked())send(channelId("mute",selectedOutput_->currentIndex()),v);});
        connect(solo, &QCheckBox::toggled, this, [this, solo](bool v){if(!solo->signalsBlocked())send(channelId("solo",selectedOutput_->currentIndex()),v);});
        connect(polarity, &QCheckBox::toggled, this, [this, polarity](bool v){if(!polarity->signalsBlocked())send(channelId("polarity",selectedOutput_->currentIndex()),v);});
        actionRow->addSpacing(20);
        applySideDelay_ = new QPushButton("APPLY L1 DELAY TO L WAYS");
        applySideDelay_->setToolTip("Copies WAY 1 RELATIVE DELAY to every active way on the selected L or R side.");
        applySideDelay_->setVisible(false);
        actionRow->addWidget(applySideDelay_);
        connect(applySideDelay_, &QPushButton::clicked, this, [this] {
            const char* rolePrefix = lastTelemetry_.crossoverCinemaProgram ? "cinemaRole" : "source";
            const int selected = selectedOutput_->currentIndex();
            const int role = qRound(parameter(lastTelemetry_, channelId(rolePrefix, selected), 0.0));
            if (role != 1 && role != 6) return;
            const auto delayId = channelId(lastTelemetry_.crossoverCinemaProgram
                ? "cinemaDelayMs" : "musicDelayMs", selected);
            const double delayMs = parameter(lastTelemetry_, delayId, 0.0);
            writeEngineCrossoverApplySideDelay(
                lastTelemetry_.crossoverCinemaProgram != 0, role == 6, delayMs);
        });
        actionRow->addStretch();
        grid->addWidget(actions, 8, 0, 1, 4);
        routingSummary_ = label("LOGICAL OUTPUT 1 selected");
        routingSummary_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        routingSummary_->setStyleSheet("color:#aeb6bf;");
        auto* interpolation=combo(grid,10,"FRACTIONAL DELAY",{"Linear (reference)","Thiran 1","Thiran 3","Thiran 5"},
            [this]{return channelId("fractionalDelayMode",selectedOutput_->currentIndex());});
        interpolation->setObjectName("fractionalDelayMode");
        interpolation->setToolTip("FP64 per output. Unity magnitude at fixed delay; phase accuracy depends on order. Short delays use the highest causal order reported below. Integer delays bypass exactly. Thiran changes crossfade for 40 ms; Linear retains the reference ramp.");
        grid->addWidget(routingSummary_, 11, 0, 1, 4);
        grid->setRowStretch(12, 1);
    }

    void buildFilterPage(const QString& tab, bool lr, bool bass)
    {
        QGridLayout* grid{}; page(tab, grid);
        grid->setVerticalSpacing(3);
        auto* heading = label(bass ? "SUB / SCOOP CROSSOVER" : (lr ? "LEFT / RIGHT CROSSOVER" : "CENTER CROSSOVER"));
        heading->setStyleSheet("font-size:15px;font-weight:700;");
        grid->addWidget(heading, 0, 0, 1, 3);
        if (lr)
        {
            copyLeftToRight_ = new QPushButton("APPLY ALL L → R");
            copyLeftToRight_->setToolTip(
                "Copies every active LEFT way state to its paired RIGHT way through the host ABI. "
                "Routing, mute, emergency mute and the inactive program scene are preserved.");
            grid->addWidget(copyLeftToRight_, 0, 3, 1, 2);
            connect(copyLeftToRight_, &QPushButton::clicked,
                    this, [this] { applyLeftToRight(); });
        }
        auto* selector = new QComboBox;
        QComboBox* side = nullptr;
        if (lr)
        {
            side = new QComboBox;
            side->addItems({"LEFT", "RIGHT"});
            for (int way = 0; way < 5; ++way)
                selector->addItem(QString("WAY %1").arg(way + 1), way);
            grid->addWidget(label("SIDE"), 1, 0);
            grid->addWidget(side, 1, 1);
            grid->addWidget(label("WAY"), 1, 2);
            grid->addWidget(selector, 1, 3);
            combo(grid, 1, "LP MAX", {"40 kHz","60 kHz"},
                  []{return "lrBandLimit";}, {0,1}, 4);
        }
        else if (bass)
        {
            selector->addItem("SUB 1", 16);
            selector->addItem("SUB 2", 18);
            selector->addItem("SCOOP 1", 17);
            selector->addItem("SCOOP 2", 19);
            bassLaneSelector_ = selector;
        }
        else
        {
            selector->addItem("WAY 1", 0);
            selector->addItem("WAY 2", 1);
        }
        if (!lr)
        {
            grid->addWidget(label(bass ? "OUTPUT" : "WAY"), 1, 0);
            grid->addWidget(selector, 1, 1);
        }
        const auto channel = [this, selector, side, lr, bass] {
            return selectedPhysicalChannel(selector, side, lr, bass);
        };
        const int processRow = bass ? 1 : 2;
        const int processColumn = bass ? 4 : 0;
        auto* processBox = combo(grid, processRow, "PROCESS", {"IIR","FIR LINEAR PHASE"},
              [channel]{return channelId("processMode", channel());}, {0,1}, processColumn);
        auto* firBox = combo(grid, processRow, "FIR", {"2049 taps","4097 taps","8193 taps"},
              [channel]{return channelId("firLength", channel());}, {0,1,2}, bass ? 6 : 2);

        if (bass)
        {
            bassFeedBox_ = combo(grid, 1, "SOURCE",
                {"L+R", "L+R+C", "LEFT", "RIGHT", "CENTER"},
                [this] { return selectedBassLaneIsScoop() ? "scoopFeed" : "bassFeed"; },
                {0,1,2,3,4}, 2);
            bassLinkBox_ = combo(grid, 2, "LINK",
                {"INDEPENDENT", "COMMON SOURCE", "COMMON EXTRACTOR", "FULL LINK"},
                [] { return "bassLinkMode"; }, {0,1,2,3}, 0);
            bassExtractorBox_ = combo(grid, 2, "EXTRACTOR",
                {"CLASSIC MATRIX", "AVERAGED MATRIX", "PHASE SAFE", "COMPLEX MULTIBAND PCA"},
                [this] { return selectedBassLaneIsScoop() ? "scoopExtractor" : "bassExtractor"; },
                {0,1,2,3}, 2);
            cinemaScoopSourceBox_ = combo(grid, 2, "CINEMA SCOOP",
                {"L/R/C EXTRACT", "LFE", "L/R/C + LFE"},
                [] { return "cinemaScoopSource"; }, {0,1,2}, 4);
            cinemaLfeFilterBypassButton_ = toggle(grid, 2, "LFE FILTER BYPASS",
                [] { return "cinemaLfeFilterBypass"; }, 6);
            cinemaScoopSourceBox_->setToolTip(
                "Cinema-only SCOOP source: decoded L/R/C extractor, discrete LFE, or their direct sum.");
            cinemaLfeFilterBypassButton_->setToolTip(
                "Cinema SUB diagnostic bypass: skips HP/LP and all-pass; gain, delay and Bandwidth Protector remain active.");
            bassStatus_ = label("SUB / SCOOP CONTROL STATUS");
            bassStatus_->setStyleSheet(
                "color:#aeb6bf;background:#11171e;border:1px solid #303945;padding:2px 6px;");
            grid->addWidget(bassStatus_, 3, 0, 1, 8);
        }

        const int algorithmRow = bass ? 4 : 2;
        const int hpRow = bass ? 4 : 3;
        const int lpRow = bass ? 5 : 4;
        const int apRow = bass ? 6 : 5;
        auto* allPassButton = toggle(grid, apRow, "ALL-PASS ALIGN",
            [channel]{return channelId("allPass", channel());}, bass ? 0 : 4);
        auto* algorithmBox = combo(grid, algorithmRow, "ALGORITHM",
            {"BW12","BW24","LR24","LR48","BW36","BW48"},
            [channel]{return channelId("filterType", channel());}, {0,1,2,3,4,5},
            bass ? 0 : 5);
        auto* highPassButton = toggle(grid, hpRow, "HIGH-PASS",
            [channel]{return channelId("hpOn", channel());}, bass ? 2 : 0);
        auto* highPassSlider = slider(grid, hpRow, "HP FREQUENCY", 2,
            bass ? 5000 : 60000, bass ? 20 : 80, 10, 1, " Hz",
            [channel]{return channelId("hpHz", channel());}, bass ? 3 : 1,
            bass ? 4 : 5);
        auto* lowPassButton = toggle(grid, lpRow, "LOW-PASS",
            [channel]{return channelId("lpOn", channel());}, 0);
        auto* lowPassSlider = slider(
            grid, lpRow, "LP FREQUENCY", 2,
            bass ? 1000 : (lr ? 60000 : 20000),
            bass ? 200 : 20000, 10, 1, " Hz",
            [channel]{return channelId("lpHz", channel());}, bass ? 1 : 1,
            bass ? 6 : 5);
        auto* apFrequencySlider = slider(grid, apRow, "AP FREQ", 2,
            bass ? 1000 : 60000, bass ? 80 : 1000, 10, 1, " Hz",
            [channel]{return channelId("apFreq", channel());}, bass ? 2 : 0,
            bass ? 2 : 2);
        auto* apQSlider = slider(grid, apRow, "AP Q", 0.2, 4, 0.707, 100, 2, "",
            [channel]{return channelId("apQ", channel());}, bass ? 5 : 3,
            bass ? 2 : 2);
        auto* plotRow = new QWidget;
        auto* plotLayout = new QHBoxLayout(plotRow);
        plotLayout->setContentsMargins(0, 0, 0, 0);
        plotLayout->setSpacing(6);
        auto* graph = new CrossoverResponseGraph;
        auto* meters = new ThinMeterBank;
        plotLayout->addWidget(graph, 1);
        plotLayout->addWidget(meters);
        const int plotRowIndex = bass ? 7 : 6;
        grid->addWidget(plotRow, plotRowIndex, 0, 1, bass ? 8 : 7);
        grid->setRowStretch(plotRowIndex, 1);
        for (int column = 1; column < (bass ? 8 : 7); column += 2)
            grid->setColumnStretch(column, 1);
        filterPages_.push_back({selector, side, lowPassSlider, graph, meters, lr, bass});
        if (bass)
        {
            bassProcessBox_ = processBox;
            bassFirBox_ = firBox;
            bassAllPassButton_ = allPassButton;
            bassAlgorithmBox_ = algorithmBox;
            bassHighPassButton_ = highPassButton;
            bassHighPassSlider_ = highPassSlider;
            bassLowPassButton_ = lowPassButton;
            bassLowPassSlider_ = lowPassSlider;
            bassApFrequencySlider_ = apFrequencySlider;
            bassApQSlider_ = apQSlider;
        }
        graph->onCutoffGesture = [](int physicalChannel, bool highPass, bool begin) {
            sendGesture(channelId(highPass ? "hpHz" : "lpHz", physicalChannel), begin);
        };
        graph->onCutoffDragged = [](int physicalChannel, bool highPass, double frequency) {
            send(channelId(highPass ? "hpHz" : "lpHz", physicalChannel), frequency);
        };
        const auto monitorSelection = [this, selector, side, lr, bass] {
            writeEngineCrossoverMonitorChannel(selectedPhysicalChannel(selector, side, lr, bass));
        };
        connect(selector, &QComboBox::currentIndexChanged, this, monitorSelection);
        if (side != nullptr)
            connect(side, &QComboBox::currentIndexChanged, this, monitorSelection);
        if (!lr && !bass)
        {
            auto* note = label("CROSSOVER OF MUSIC_C — CENTER EXTRACTION IS A SEPARATE MODULE");
            note->setStyleSheet("color:#65c7d9;font-size:10px;");
            grid->addWidget(note, 0, 2, 1, 4);
        }
    }

    void buildCinema()
    {
        QGridLayout* grid{}; page("MUSIC/CINEMA", grid);
        grid->setVerticalSpacing(8);
        cinemaHeading_ = label("MUSIC L/R/C INPUT / PROGRAM INTERLOCK");
        cinemaHeading_->setStyleSheet("font-size:18px;font-weight:700;");
        grid->addWidget(cinemaHeading_, 0, 0, 1, 10);

        auto* controls = new QWidget;
        auto* controlGrid = new QGridLayout(controls);
        controlGrid->setContentsMargins(0, 0, 0, 0);
        cinemaProgramBox_ = combo(controlGrid, 0, "ACTIVE PROGRAM",
            {"MUSIC L/R/C", "CINEMA 7.1.2"}, []{return "cinemaSource";}, {0,1}, 0);
        cinemaLfeReferenceButton_ = toggle(controlGrid, 0, "LFE +10 dB REFERENCE",
            []{return "cinemaLfeReferenceGain";}, 2);
        cinemaProgramStatus_ = label("BUS: ACTIVE / 3 CH   |   SOURCE: MUSIC");
        cinemaProgramStatus_->setAlignment(Qt::AlignCenter);
        controlGrid->addWidget(cinemaProgramStatus_, 0, 3);
        controlGrid->setColumnStretch(3, 1);
        grid->addWidget(controls, 1, 0, 1, 10);

        auto* lanesWidget = new QWidget;
        auto* lanes = new QHBoxLayout(lanesWidget);
        cinemaLaneLayout_ = lanes;
        lanes->setContentsMargins(0, 0, 0, 0);
        lanes->setSpacing(5);
        const QStringList names{
            "ML LEFT", "MR RIGHT", "MC CENTER", "LFE", "CSL SIDE LEFT",
            "CSR SIDE RIGHT", "CBL BACK LEFT", "CBR BACK RIGHT",
            "CTSL TOP SIDE LEFT", "CTSR TOP SIDE RIGHT"};
        for (int channel = 0; channel < 10; ++channel)
        {
            auto* frame = new QFrame;
            frame->setMinimumWidth(62);
            cinemaLaneFrames_[size_t(channel)] = frame;
            auto* lane = new QVBoxLayout(frame);
            lane->setContentsMargins(5, 5, 5, 5);
            lane->setSpacing(4);
            auto* name = label(names[channel]);
            name->setAlignment(Qt::AlignCenter);
            cinemaLaneLabels_[size_t(channel)] = name;
            lane->addWidget(name);
            auto* pair = new QHBoxLayout;
            pair->setSpacing(4);
            auto* trim = new QSlider(Qt::Vertical);
            cinemaTrimSliders_[size_t(channel)] = trim;
            trim->setRange(-240,240);
            trim->setValue(0);
            trim->setInvertedAppearance(false);
            trim->setInvertedControls(false);
            auto* meter = new QProgressBar;
            meter->setRange(0,720);
            meter->setOrientation(Qt::Vertical);
            meter->setTextVisible(false);
            meter->setFixedWidth(8);
            meter->setStyleSheet("QProgressBar{border:0;background:#0c1117;}QProgressBar::chunk{background:#ffc400;}");
            pair->addWidget(meter, 0, Qt::AlignHCenter);
            pair->addWidget(trim, 1, Qt::AlignHCenter);
            lane->addLayout(pair, 1);
            auto* value = label("0.0 dB");
            cinemaLaneValues_[size_t(channel)] = value;
            value->setAlignment(Qt::AlignCenter);
            lane->addWidget(value);
            cinemaMeters_[channel] = meter;
            lanes->addWidget(frame, 0);
            sliderBindings_.push_back({trim, value, QString{}, " dB", 10, 1,
                [this, channel]{return channelId(lastTelemetry_.crossoverCinemaProgram
                    ? "cinemaTrimDb" : "musicTrimDb", channel);}});
            connect(trim, &QSlider::valueChanged, this, [this, trim, channel](int raw){
                if(!trim->signalsBlocked()) send(channelId(lastTelemetry_.crossoverCinemaProgram
                    ? "cinemaTrimDb" : "musicTrimDb", channel), raw/10.0);
            });
            connect(trim, &QSlider::sliderPressed, this,
                [this, channel] { sendGesture(channelId(lastTelemetry_.crossoverCinemaProgram
                    ? "cinemaTrimDb" : "musicTrimDb", channel), true); });
            connect(trim, &QSlider::sliderReleased, this,
                [this, channel] { sendGesture(channelId(lastTelemetry_.crossoverCinemaProgram
                    ? "cinemaTrimDb" : "musicTrimDb", channel), false); });
        }
        // MUSIC uses only L/R/C and keeps the three strips compact at the left.
        // CINEMA changes the stretch factors at refresh time so all ten lanes
        // share the available width. The final stretch owns unused MUSIC space.
        lanes->addStretch(1);
        grid->addWidget(lanesWidget, 2, 0, 1, 10);
        cinemaDescription_ = label(
            "MUSIC inputs: ML, MR and optional MC. Trims and meters are post-input and pre-crossover. "
            "SUB and SCOOP are derived only from the selected MUSIC feed and extractor; the CINEMA "
            "LFE lane and its +10 dB option are isolated.");
        cinemaDescription_->setWordWrap(true);
        cinemaDescription_->setStyleSheet("color:#aeb6bf;");
        grid->addWidget(cinemaDescription_, 3, 0, 1, 10);
        grid->setRowStretch(2, 1);
    }

    void buildModule()
    {
        QGridLayout* grid{}; page("MODULE", grid);
        auto* heading = label("CROSSOVER MODULE STATUS");
        heading->setStyleSheet("font-size:18px;font-weight:700;");
        grid->addWidget(heading, 0, 0, 1, 2);
        system_ = label("MODULE STATUS UNAVAILABLE");
        system_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        system_->setStyleSheet("font-size:13px;color:#d4d9df;");
        grid->addWidget(system_, 1, 0, 1, 2);
        auto* ownership = label(
            "OWNERSHIP BOUNDARY\n\n"
            "Crossover: semantic roles, filters, FIR/IIR, all-pass, acoustic gain and delay.\n"
            "Host: graph lifecycle, project state and control transactions.\n"
            "Output Matrix: logical role → DAC/channel.\n"
            "Guardian / PCM24 / DAC Sink: independent downstream modules.");
        ownership->setWordWrap(true);
        ownership->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        ownership->setStyleSheet("background:#0c1117;border:1px solid #303945;padding:12px;color:#aeb6bf;");
        grid->addWidget(ownership, 2, 0, 1, 2);
        grid->setRowStretch(3, 1);
    }

    void buildBandwidthProtector()
    {
        QGridLayout* grid{}; page("BANDWIDTH PROTECTOR", grid);
        auto* heading = label("BANDWIDTH PROTECTOR / CROSSOVER-OWNED");
        heading->setStyleSheet("font-size:18px;font-weight:700;color:#ffca58;");
        grid->addWidget(heading, 0, 0, 1, 4);
        auto* description = label(
            "Validates only the configured HP/LP edges of each logical output against "
            "SAFE MIN / SAFE MAX. It never analyses level, limits, clamps or changes gain. "
            "After a latched trip its only audio action is MUTE for that output.");
        description->setWordWrap(true);
        description->setStyleSheet(
            "background:#11171e;border:1px solid #5f4b18;padding:9px;color:#d8c995;");
        grid->addWidget(description, 1, 0, 1, 4);

        grid->addWidget(label("LOGICAL OUTPUT"), 2, 0);
        protectorSelector_ = new QComboBox;
        protectorSelector_->addItems(channelNames());
        grid->addWidget(protectorSelector_, 2, 1);
        protectorStatus_ = label("STATE UNAVAILABLE");
        protectorStatus_->setAlignment(Qt::AlignCenter);
        protectorStatus_->setStyleSheet(
            "background:#11171e;border:1px solid #39434d;padding:6px;font-weight:700;");
        grid->addWidget(protectorStatus_, 2, 2, 1, 2);

        toggle(grid, 3, "ENABLE BANDWIDTH TRIP", [this] {
            return channelId("protectorOn", protectorSelector_->currentIndex());
        });
        slider(grid, 4, "SAFE MIN", 2.0, 60000.0, 2.0, 1.0, 1, " Hz", [this] {
            return channelId("safetyMinHz", protectorSelector_->currentIndex());
        }, 0, 2);
        slider(grid, 5, "SAFE MAX", 2.0, 96000.0, 96000.0, 1.0, 1, " Hz", [this] {
            return channelId("safetyMaxHz", protectorSelector_->currentIndex());
        }, 0, 2);
        slider(grid, 6, "TRIP HOLD", 5.0, 1000.0, 50.0, 1.0, 0, " ms", [this] {
            return channelId("tripHoldMs", protectorSelector_->currentIndex());
        }, 0, 2);

        protectorReset_ = new QPushButton("RESET SELECTED TRIP");
        protectorResetAll_ = new QPushButton("RESET ALL TRIPS");
        protectorResetAll_->setStyleSheet(
            "QPushButton{border:1px solid #a96d00;color:#ffca58;background:#241b08;}"
            "QPushButton:hover{border-color:#ffca58;color:white;}");
        grid->addWidget(protectorReset_, 7, 0, 1, 2);
        grid->addWidget(protectorResetAll_, 7, 2, 1, 2);
        connect(protectorReset_, &QPushButton::clicked, this, [this] {
            if (protectorSelector_ != nullptr)
                writeEngineCrossoverResetProtectionTrip(
                    static_cast<std::uint32_t>(protectorSelector_->currentIndex()));
        });
        connect(protectorResetAll_, &QPushButton::clicked, this, [] {
            writeEngineCrossoverResetAllProtectionTrips();
        });
        grid->setRowStretch(8, 1);
    }

    void refreshFilterPages(const EngineTelemetrySnapshot& telemetry)
    {
        int desiredMonitor = -1;
        for (std::size_t pageIndex = 0; pageIndex < filterPages_.size(); ++pageIndex)
        {
            auto& page = filterPages_[pageIndex];
            const int lrWays = std::clamp(
                qRound(parameter(telemetry, "lrWays", 1.0)), 1, 5);
            const int centerWays = std::clamp(
                qRound(parameter(telemetry, "centerWays", 1.0)), 1, 2);
            const int subCount = std::clamp(
                qRound(parameter(telemetry, "subSpeakers", 1.0)), 1, 2);
            const int scoopCount = std::clamp(
                qRound(parameter(telemetry, "scoopSpeakers", 1.0)), 1, 2);
            std::vector<std::pair<QString, int>> selectable;
            if (page.lr)
                for (int way = 0; way < lrWays; ++way)
                    selectable.push_back({QString("WAY %1").arg(way + 1), way});
            else if (page.bass)
            {
                selectable.push_back({"SUB 1", 16});
                if (subCount == 2) selectable.push_back({"SUB 2", 18});
                selectable.push_back({"SCOOP 1", 17});
                if (scoopCount == 2) selectable.push_back({"SCOOP 2", 19});
            }
            else
                for (int way = 0; way < centerWays; ++way)
                    selectable.push_back({QString("WAY %1").arg(way + 1), way});
            setSelectorItems(page.selector, selectable);
            const int selected = selectedPhysicalChannel(
                page.selector, page.side, page.lr, page.bass);
            if (page.bass)
            {
                const int role = page.selector->currentData().toInt();
                const bool sub = role == 16 || role == 18;
                const QSignalBlocker blocker(page.lowPassSlider);
                page.lowPassSlider->setMaximum((sub ? 200 : 1000) * 10);
                page.lowPassSlider->setToolTip(sub
                    ? "SUB low-pass safety limit: 200 Hz"
                    : "SCOOP low-pass safety limit: 1 kHz");
                refreshBassExtractionControls(telemetry, selected, sub);
            }
            if (tabs_->currentIndex() == static_cast<int>(pageIndex) + 1)
                desiredMonitor = selected;
            std::vector<std::pair<int, QString>> channels;
            if (page.lr)
            {
                const bool right = page.side != nullptr && page.side->currentIndex() == 1;
                for (int way = 1; way <= lrWays; ++way)
                {
                    const int channel = channelForRole(
                        telemetry, (right ? 5 : 0) + way);
                    if (channel >= 0)
                        channels.push_back({channel, QString("W%1").arg(way)});
                }
            }
            else if (page.bass)
            {
                const std::array<std::pair<int, QString>, 4> roles {{
                    {16, "S1"}, {18, "S2"}, {17, "C1"}, {19, "C2"}
                }};
                for (int index = 0; index < 4; ++index)
                {
                    if ((index == 1 && subCount < 2) ||
                        (index == 3 && scoopCount < 2))
                        continue;
                    const int channel = channelForRole(telemetry, roles[size_t(index)].first);
                    if (channel >= 0)
                        channels.push_back({channel, roles[size_t(index)].second});
                }
            }
            else
            {
                for (int way = 1; way <= centerWays; ++way)
                {
                    const int channel = channelForRole(telemetry, 10 + way);
                    if (channel >= 0)
                        channels.push_back({channel, QString("W%1").arg(way)});
                }
            }

            std::vector<CrossoverResponseGraph::Curve> curves;
            std::vector<ThinMeterBank::Meter> meters;
            for (std::size_t index = 0; index < channels.size(); ++index)
            {
                CrossoverResponseGraph::Curve curve;
                curve.channel = channels[index].first;
                curve.label = channels[index].second;
                curve.colour = curveColours[index % curveColours.size()];
                std::copy_n(telemetry.crossoverResponseDb[curve.channel],
                            crossoverResponsePointCount, curve.decibels.begin());
                curves.push_back(curve);
                meters.push_back({curve.label, curve.colour,
                                  telemetry.crossoverOutputPeaks[curve.channel]});
            }
            page.meters->setMeters(std::move(meters));
            const double maximumHz = page.bass ? 5000.0
                : (page.lr && qRound(parameter(telemetry, "lrBandLimit", 0.0)) == 1
                    ? 60000.0 : 40000.0);
            const double hpHz = selected >= 0
                ? parameter(telemetry, channelId("hpHz", selected), 20.0) : 20.0;
            const double lpHz = selected >= 0
                ? parameter(telemetry, channelId("lpHz", selected), maximumHz)
                : maximumHz;
            const bool spectrumForSelection =
                telemetry.crossoverViewTelemetryAvailable != 0 &&
                telemetry.crossoverMonitoredChannel == selected &&
                telemetry.crossoverSpectrumPoints > 0;
            page.graph->setData(
                std::move(curves), selected, 20.0, maximumHz, hpHz, lpHz,
                telemetry.crossoverSpectrumFrequencyHz,
                telemetry.crossoverSpectrumDb,
                spectrumForSelection ? telemetry.crossoverSpectrumPoints : 0u,
                telemetry.crossoverViewTelemetryAvailable != 0);
        }
        if (desiredMonitor != requestedMonitorChannel_)
        {
            writeEngineCrossoverMonitorChannel(desiredMonitor);
            requestedMonitorChannel_ = desiredMonitor;
        }
    }

    void refreshBassExtractionControls(const EngineTelemetrySnapshot& telemetry,
                                       int selectedChannel, bool sub)
    {
        if (bassFeedBox_ == nullptr || bassExtractorBox_ == nullptr ||
            bassLinkBox_ == nullptr || cinemaScoopSourceBox_ == nullptr ||
            cinemaLfeFilterBypassButton_ == nullptr || bassStatus_ == nullptr)
            return;

        const bool controlsAvailable = telemetry.crossoverControlAvailable != 0;
        const bool cinema = telemetry.crossoverCinemaProgram != 0;
        const bool scoop = !sub;
        const int linkMode = std::clamp(
            qRound(parameter(telemetry, "bassLinkMode", 0.0)), 0, 3);
        const int cinemaScoopMode = std::clamp(
            qRound(parameter(telemetry, "cinemaScoopSource", 0.0)), 0, 2);
        const bool cinemaDirectLfe = cinema && sub;
        const bool cinemaScoopUsesLrc = cinemaScoopMode != 1;

        QString feedParameter = scoop ? QStringLiteral("scoopFeed")
                                      : QStringLiteral("bassFeed");
        QString extractorParameter = scoop ? QStringLiteral("scoopExtractor")
                                           : QStringLiteral("bassExtractor");
        if (!cinema && scoop && linkMode > 0)
            feedParameter = QStringLiteral("bassFeed");
        if (!cinema && scoop && linkMode >= 2)
            extractorParameter = QStringLiteral("bassExtractor");

        const int feedIndex = std::clamp(
            qRound(parameter(telemetry, feedParameter, 0.0)), 0, 4);
        const int extractorIndex = std::clamp(
            qRound(parameter(telemetry, extractorParameter, 1.0)), 0, 3);
        {
            const QSignalBlocker blocker(bassFeedBox_);
            bassFeedBox_->setCurrentIndex(feedIndex);
        }
        {
            const QSignalBlocker blocker(bassExtractorBox_);
            bassExtractorBox_->setCurrentIndex(extractorIndex);
        }

        const bool sourceEnabled = controlsAvailable && !cinemaDirectLfe &&
            (!cinema || !scoop || cinemaScoopUsesLrc) &&
            (cinema || !scoop || linkMode == 0);
        const bool extractorEnabled = controlsAvailable && !cinemaDirectLfe &&
            (!cinema || !scoop || cinemaScoopUsesLrc) &&
            (cinema || !scoop || linkMode < 2);
        bassFeedBox_->setEnabled(sourceEnabled);
        bassExtractorBox_->setEnabled(extractorEnabled);
        bassLinkBox_->setEnabled(controlsAvailable && !cinema);
        cinemaScoopSourceBox_->setEnabled(controlsAvailable && cinema && scoop);
        cinemaLfeFilterBypassButton_->setEnabled(
            controlsAvailable && cinemaDirectLfe);

        const bool filterBypass = cinemaDirectLfe &&
            parameter(telemetry, "cinemaLfeFilterBypass", 0.0) >= 0.5;
        const bool filterControlsEnabled = controlsAvailable &&
            selectedChannel >= 0 && !filterBypass;
        const int processMode = selectedChannel >= 0
            ? std::clamp(qRound(parameter(telemetry,
                channelId("processMode", selectedChannel), 0.0)), 0, 1) : 0;
        const bool allPass = selectedChannel >= 0 &&
            parameter(telemetry, channelId("allPass", selectedChannel), 0.0) >= 0.5;
        bassProcessBox_->setEnabled(filterControlsEnabled);
        bassFirBox_->setEnabled(filterControlsEnabled && processMode == 1);
        bassAlgorithmBox_->setEnabled(filterControlsEnabled && processMode == 0);
        bassHighPassButton_->setEnabled(filterControlsEnabled);
        bassHighPassSlider_->setEnabled(filterControlsEnabled);
        bassLowPassButton_->setEnabled(filterControlsEnabled);
        bassLowPassSlider_->setEnabled(filterControlsEnabled);
        bassAllPassButton_->setEnabled(filterControlsEnabled);
        bassApFrequencySlider_->setEnabled(filterControlsEnabled && allPass);
        bassApQSlider_->setEnabled(filterControlsEnabled && allPass);

        static const QStringList feedNames {
            "L+R", "L+R+C", "LEFT", "RIGHT", "CENTER"
        };
        static const QStringList extractorNames {
            "CLASSIC MATRIX", "AVERAGED MATRIX", "PHASE SAFE",
            "COMPLEX MULTIBAND PCA"
        };
        static const QStringList linkNames {
            "INDEPENDENT", "COMMON SOURCE", "COMMON EXTRACTOR", "FULL LINK"
        };
        static const QStringList cinemaScoopNames {
            "L/R/C EXTRACT", "LFE", "L/R/C + LFE"
        };
        const QString laneName = bassLaneSelector_ != nullptr
            ? bassLaneSelector_->currentText() : QStringLiteral("BASS");
        QString status = QString("%1  →  %2")
            .arg(laneName,
                 selectedChannel >= 0 ? QString("CH %1").arg(selectedChannel + 1)
                                      : QString::fromLatin1(
                                            ui::crossoverRoleAssignmentText(false).data()));
        if (cinemaDirectLfe)
            status += QStringLiteral("   SOURCE: DIRECT CINEMA LFE   EXTRACTOR: BYPASSED");
        else
        {
            status += QString("   SOURCE: %1   EXTRACTOR: %2")
                .arg(feedNames[feedIndex], extractorNames[extractorIndex]);
            if (!cinema)
                status += QString("   LINK: %1").arg(linkNames[linkMode]);
            else if (scoop)
                status += QString("   CINEMA SCOOP: %1")
                    .arg(cinemaScoopNames[cinemaScoopMode]);
        }
        status += filterBypass
            ? QStringLiteral("   DSP: FILTER BYPASS")
            : QString("   DSP: %1").arg(processMode == 0
                ? QStringLiteral("IIR") : QStringLiteral("FIR LINEAR PHASE"));
        bassStatus_->setText(status);
        bassStatus_->setStyleSheet(filterBypass
            ? "color:#ffca58;background:#11171e;border:1px solid #ffca58;padding:2px 6px;"
            : "color:#aeb6bf;background:#11171e;border:1px solid #303945;padding:2px 6px;");
    }

    void refreshRoutingTiles(const EngineTelemetrySnapshot& telemetry)
    {
        const int lrWays = std::clamp(qRound(parameter(telemetry, "lrWays", 1.0)), 1, 5);
        const int centerWays = std::clamp(qRound(parameter(telemetry, "centerWays", 1.0)), 1, 2);
        const int subCount = std::clamp(qRound(parameter(telemetry, "subSpeakers", 1.0)), 1, 2);
        const int scoopCount = std::clamp(qRound(parameter(telemetry, "scoopSpeakers", 1.0)), 1, 2);
        const QStringList names {
            "OFF", "L WAY 1", "L WAY 2", "L WAY 3", "L WAY 4", "L WAY 5",
            "R WAY 1", "R WAY 2", "R WAY 3", "R WAY 4", "R WAY 5",
            "CENTER WAY 1", "CENTER WAY 2", "CENTER WAY 3", "CENTER WAY 4", "CENTER WAY 5",
            "SUB 1", "SCOOP 1", "SUB 2", "SCOOP 2",
            "CINEMA SL", "CINEMA SR", "CINEMA BL", "CINEMA BR", "CINEMA TSL", "CINEMA TSR"
        };
        const char* prefix = telemetry.crossoverCinemaProgram ? "cinemaRole" : "source";
        for (int channel = 0; channel < 16; ++channel)
        {
            const int role = qRound(parameter(telemetry, channelId(prefix, channel), 0.0));
            bool active = false;
            if (role >= 1 && role <= 5) active = role <= lrWays;
            else if (role >= 6 && role <= 10) active = role - 5 <= lrWays;
            else if (role >= 11 && role <= 15) active = role - 10 <= centerWays;
            else if (role == 16 || role == 17) active = true;
            else if (role == 18) active = subCount == 2;
            else if (role == 19) active = scoopCount == 2;
            else if (role >= 20 && role <= 25) active = true;
            QColor colour(0x11, 0x14, 0x17);
            if (role >= 1 && role <= 5) colour = QColor(0x48, 0xd6, 0xff);
            else if (role >= 6 && role <= 10) colour = QColor(0xff, 0xc8, 0x57);
            else if (role >= 11 && role <= 15) colour = QColor(0x7e, 0xe7, 0x87);
            else if (role == 16 || role == 18) colour = QColor(0xd2, 0xa8, 0xff);
            else if (role == 17 || role == 19) colour = QColor(0xff, 0x7b, 0x72);
            else if (role >= 20 && role <= 25) colour = QColor(0x4f, 0xd1, 0xc5);
            auto* tile = routingTiles_[size_t(channel)];
            const QString roleName = role >= 0 && role < names.size() ? names[role] : QStringLiteral("OFF");
            tile->setText(QString("CH%1   %2").arg(channel + 1).arg(roleName));
            tile->setChecked(channel == selectedOutput_->currentIndex());
            const QColor background = active ? colour.darker(260) : QColor(0x11, 0x14, 0x17);
            const QColor border = active ? colour : QColor(0x30, 0x39, 0x45);
            tile->setStyleSheet(QString(
                "QPushButton{background:%1;border:1px solid %2;color:%3;padding:3px;}"
                "QPushButton:checked{border:2px solid %2;background:%4;}")
                .arg(background.name(), border.name(),
                     active ? QStringLiteral("#ffffff") : QStringLiteral("#626a73"),
                     active ? colour.darker(190).name() : QStringLiteral("#252b31")));
        }
        const int selected = std::clamp(selectedOutput_->currentIndex(), 0, 15);
        const int role = qRound(parameter(telemetry, channelId(prefix, selected), 0.0));
        const QString roleName = role >= 0 && role < names.size() ? names[role] : QStringLiteral("OFF");
        routingSummary_->setText(QString("LOGICAL OUTPUT %1   |   %2")
            .arg(selected + 1).arg(roleName));
        const bool sideWayOne = role == 1 || role == 6;
        applySideDelay_->setVisible(sideWayOne);
        applySideDelay_->setEnabled(sideWayOne && telemetry.crossoverControlAvailable != 0);
        if (sideWayOne)
        {
            const int ways = std::clamp(qRound(parameter(telemetry, "lrWays", 1.0)), 1, 5);
            const QString side = role == 1 ? QStringLiteral("L") : QStringLiteral("R");
            applySideDelay_->setText(QString("APPLY %1 WAY 1 DELAY TO %2 ACTIVE WAYS")
                .arg(side).arg(ways));
        }
        QColor selectedColour(0x48, 0xd6, 0xff);
        if (role >= 6 && role <= 10) selectedColour = QColor(0xff, 0xc8, 0x57);
        else if (role >= 11 && role <= 15) selectedColour = QColor(0x7e, 0xe7, 0x87);
        else if (role == 16 || role == 18) selectedColour = QColor(0xd2, 0xa8, 0xff);
        else if (role == 17 || role == 19) selectedColour = QColor(0xff, 0x7b, 0x72);
        else if (role >= 20 && role <= 25) selectedColour = QColor(0x4f, 0xd1, 0xc5);
        else if (role == 0) selectedColour = QColor(0x62, 0x6a, 0x73);
        styleChannelDelaySlider(fineDelaySlider_, selectedColour);
        styleChannelDelaySlider(coarseDelaySlider_, selectedColour);
    }

    static QStringList channelNames()
    {
        QStringList result; for(int i=1;i<=16;++i) result << QString("CH %1").arg(i); return result;
    }

    void refresh()
    {
        EngineTelemetrySnapshot telemetry{};
        if (!readEngineTelemetry(telemetry)) return;
        lastTelemetry_ = telemetry;
        lastTelemetryValid_ = true;
        runtime_->setText(QString("FP64  |  %1 kHz  |  %2 samples  |  latency %3  |  ABI v1%4")
            .arg(telemetry.crossoverSampleRate/1000.0,0,'f',1)
            .arg(telemetry.crossoverBlockSize).arg(telemetry.crossoverLatencySamples)
            .arg(telemetry.crossoverProtectionGlobalTrip ? "  |  BAND TRIP" : ""));
        runtime_->setStyleSheet(telemetry.crossoverProtectionGlobalTrip
            ? "color:#ff5c68;font-weight:800;" : "color:#aeb6bf;");
        if (telemetry.crossoverControlAvailable)
        {
            for (const auto& binding : comboBindings_)
            {
                const auto id=binding.id(); if(id.isEmpty()) continue;
                const int semanticValue = qRound(parameter(telemetry,id,binding.widget->currentIndex()));
                const int widgetIndex = binding.semanticValues.isEmpty()
                    ? semanticValue : binding.semanticValues.indexOf(semanticValue);
                const QSignalBlocker blocker(binding.widget);
                if (widgetIndex >= 0) binding.widget->setCurrentIndex(widgetIndex);
            }
            for (const auto& binding : toggleBindings_)
            {
                const QSignalBlocker blocker(binding.widget); binding.widget->setChecked(parameter(telemetry,binding.id(),binding.widget->isChecked()?1:0)>=0.5);
            }
            for (const auto& binding : sliderBindings_)
            {
                const double value=parameter(telemetry,binding.id(),binding.widget->value()/binding.scale);
                if(!binding.widget->isSliderDown()){const QSignalBlocker blocker(binding.widget);binding.widget->setValue(qRound(value*binding.scale));}
                binding.label->setText(binding.caption.isEmpty()
                    ? QString("%1%2").arg(value,0,'f',binding.decimals).arg(binding.suffix)
                    : QString("%1   %2%3").arg(binding.caption).arg(value,0,'f',binding.decimals).arg(binding.suffix));
            }
        }
        refreshDelay(telemetry);
        const bool cinemaProgram = telemetry.crossoverCinemaProgram != 0;
        cinemaHeading_->setText(cinemaProgram
            ? "CINEMA 7.1.2 INPUT / PROGRAM INTERLOCK"
            : "MUSIC L/R/C INPUT / PROGRAM INTERLOCK");
        cinemaLfeReferenceButton_->setVisible(cinemaProgram);
        cinemaLfeReferenceButton_->setEnabled(
            cinemaProgram && telemetry.crossoverControlAvailable != 0);
        cinemaLfeReferenceButton_->setToolTip(cinemaProgram
            ? "Applies the CINEMA LFE reference gain through the Crossover ABI."
            : "Available only while the CINEMA program is active.");
        cinemaLfeReferenceButton_->setStyleSheet(cinemaProgram
            ? "QCheckBox{color:#d7e5f2;}"
            : "QCheckBox{color:#52616b;}");
        cinemaProgramStatus_->setText(cinemaProgram
            ? "BUS: ACTIVE / 10 CH   |   SOURCE: CINEMA (MUSIC L/R/C BLOCKED)"
            : "BUS: ACTIVE / 3 CH   |   SOURCE: MUSIC");
        cinemaProgramStatus_->setStyleSheet(cinemaProgram
            ? "color:#d2a8ff;border:1px solid #d2a8ff;padding:4px;"
            : "color:#48d6ff;border:1px solid #48d6ff;padding:4px;");
        const QStringList musicNames{"ML LEFT", "MR RIGHT", "MC CENTER"};
        const QStringList cinemaNames{
            "CL LEFT", "CR RIGHT", "CC CENTER", "LFE", "CSL SIDE LEFT",
            "CSR SIDE RIGHT", "CBL BACK LEFT", "CBR BACK RIGHT",
            "CTSL TOP SIDE LEFT", "CTSR TOP SIDE RIGHT"};
        cinemaDescription_->setText(cinemaProgram
            ? "CINEMA input order: CL, CR, CC, LFE, CSL, CSR, CBL, CBR, CTSL, CTSR. "
              "Trims and meters are post-input and pre-crossover. LFE feeds SUB directly; optional "
              "+10 dB reference compensation affects only this discrete LFE lane."
            : "MUSIC inputs: ML, MR and optional MC. Trims and meters are post-input and pre-crossover. "
              "SUB and SCOOP are derived only from the selected MUSIC feed and extractor; the CINEMA "
              "LFE lane and its +10 dB option are isolated.");
        for (int channel = 0; channel < 10; ++channel)
        {
            const bool visible = cinemaProgram || channel < 3;
            const bool leftLane = channel == 0 || channel == 4 || channel == 6 || channel == 8;
            const bool rightLane = channel == 1 || channel == 5 || channel == 7 || channel == 9;
            const QColor laneAccent = channel == 2 ? QColor(0xff, 0xd2, 0x23)
                : channel == 3 ? QColor(0xd2, 0xa8, 0xff)
                : leftLane ? QColor(0x55, 0xd9, 0x8b)
                : rightLane ? QColor(0x48, 0xa8, 0xff)
                : QColor(0x8b, 0x98, 0xa5);
            cinemaLaneLabels_[size_t(channel)]->setText(cinemaProgram
                ? cinemaNames[channel]
                : (channel < musicNames.size() ? musicNames[channel] : QString{}));
            cinemaLaneFrames_[size_t(channel)]->setVisible(visible);
            cinemaLaneFrames_[size_t(channel)]->setMinimumWidth(cinemaProgram ? 62 : 128);
            cinemaLaneFrames_[size_t(channel)]->setMaximumWidth(cinemaProgram ? QWIDGETSIZE_MAX : 160);
            cinemaLaneLayout_->setStretch(channel, cinemaProgram ? 1 : 0);
            cinemaLaneFrames_[size_t(channel)]->setStyleSheet(QString(
                "QFrame{background:#11171e;border:1px solid %1;border-radius:4px;}")
                .arg(laneAccent.name()));
            cinemaTrimSliders_[size_t(channel)]->setStyleSheet(QString(
                "QSlider::groove:vertical{width:3px;background:#202932;border:0;}"
                "QSlider::sub-page:vertical{background:#202932;}"
                "QSlider::add-page:vertical{background:%1;}"
                "QSlider::handle:vertical{height:8px;margin:0 -4px;background:%1;"
                "border:1px solid %1;border-radius:4px;}")
                .arg(laneAccent.name()));
            cinemaMeters_[size_t(channel)]->setStyleSheet(QString(
                "QProgressBar{border:0;background:#0c1117;}"
                "QProgressBar::chunk{background:%1;}")
                .arg(laneAccent.name()));
            cinemaLaneValues_[size_t(channel)]->setStyleSheet(QString(
                "color:%1;border:1px solid %1;padding:2px;")
                .arg(laneAccent.name()));
            if (!visible) continue;
            const double peak = cinemaProgram
                ? telemetry.crossoverCinemaInputPeaks[channel]
                : telemetry.crossoverMusicInputPeaks[channel];
            const double db = peak > 1.0e-12
                ? std::max(-72.0, 20.0 * std::log10(peak)) : -72.0;
            cinemaMeters_[size_t(channel)]->setValue(qRound((db + 72.0) * 10.0));
        }
        cinemaLaneLayout_->setStretch(10, cinemaProgram ? 0 : 1);
        refreshRoutingTiles(telemetry);
        refreshFilterPages(telemetry);
        if (protectorSelector_ != nullptr && protectorStatus_ != nullptr)
        {
            const int channel = std::clamp(protectorSelector_->currentIndex(), 0, 15);
            const auto state = telemetry.crossoverProtectionState[channel];
            const bool available = telemetry.crossoverProtectionAvailable != 0;
            QString text = "ABI UNAVAILABLE";
            QString colour = "#626a73";
            if (available)
            {
                if (state == ENTROPY_CROSSOVER_PROTECTION_ARMED_TRIPPED)
                { text = "TRIPPED / OUTPUT MUTED"; colour = "#ff5c68"; }
                else if (state == ENTROPY_CROSSOVER_PROTECTION_ARMED_WARNING)
                { text = "WARNING / HOLD ACTIVE"; colour = "#ffca58"; }
                else if (state == ENTROPY_CROSSOVER_PROTECTION_DISABLED_WARNING)
                { text = "DISABLED / BAND INVALID"; colour = "#ff9f43"; }
                else if (state == ENTROPY_CROSSOVER_PROTECTION_DISABLED_SAFE)
                { text = "DISABLED / BAND VALID"; colour = "#71808f"; }
                else
                { text = "ARMED / BAND VALID"; colour = "#55d98b"; }
            }
            protectorStatus_->setText(QString("CH%1  •  %2").arg(channel + 1).arg(text));
            protectorStatus_->setStyleSheet(QString(
                "background:#11171e;border:1px solid %1;color:%1;padding:6px;font-weight:800;")
                .arg(colour));
            protectorReset_->setEnabled(available &&
                (telemetry.crossoverProtectionTrippedMask & (1u << channel)) != 0u);
            protectorResetAll_->setEnabled(available &&
                telemetry.crossoverProtectionTrippedMask != 0u);
        }
        if (copyLeftToRight_ != nullptr &&
            copyLeftToRight_->text() == QStringLiteral("APPLY ALL L → R"))
            copyLeftToRight_->setEnabled(telemetry.crossoverControlAvailable != 0);
        system_->setText(QString(
            "audio.entropy.crossover\n\n"
            "Precision: FP64\n"
            "Common ABI: 0x00010000\n"
            "Control ABI: %1\nTelemetry ABI: %2\nView Telemetry ABI: %3\nProtection ABI: %4\n\n"
            "Sample rate: %5 kHz\nBlock size: %6 samples\nLatency: %7 samples\n"
            "Semantic inputs: 13\nLogical outputs: up to 16\nProgram: %8")
            .arg(telemetry.crossoverControlAvailable ? "AVAILABLE" : "UNAVAILABLE")
            .arg(telemetry.crossoverTelemetryAvailable ? "AVAILABLE" : "TELEMETRY UNAVAILABLE")
            .arg(telemetry.crossoverViewTelemetryAvailable ? "AVAILABLE" : "RESPONSE DATA UNAVAILABLE")
            .arg(telemetry.crossoverProtectionAvailable ? "AVAILABLE" : "UNAVAILABLE")
            .arg(telemetry.crossoverSampleRate/1000.0,0,'f',1).arg(telemetry.crossoverBlockSize).arg(telemetry.crossoverLatencySamples)
            .arg(telemetry.crossoverCinemaProgram?"CINEMA 7.1.2":"MUSIC L/R/C"));
    }

    QTabWidget* tabs_{}; QLabel* runtime_{}; QLabel* system_{};
    QComboBox* selectedOutput_{};
    QLabel* routingSummary_{};
    QLabel* fineDelayLabel_{};
    QSlider* fineDelaySlider_{};
    QSlider* coarseDelaySlider_{};
    QComboBox* fineRatio_{};
    QPushButton* applySideDelay_{};
    QPushButton* copyLeftToRight_{};
    QComboBox* protectorSelector_{};
    QLabel* protectorStatus_{};
    QPushButton* protectorReset_{};
    QPushButton* protectorResetAll_{};
    QComboBox* cinemaProgramBox_{};
    QCheckBox* cinemaLfeReferenceButton_{};
    QLabel* cinemaHeading_{};
    QLabel* cinemaProgramStatus_{};
    QLabel* cinemaDescription_{};
    QHBoxLayout* cinemaLaneLayout_{};
    QComboBox* bassLaneSelector_{};
    QComboBox* bassFeedBox_{};
    QComboBox* bassLinkBox_{};
    QComboBox* bassExtractorBox_{};
    QComboBox* cinemaScoopSourceBox_{};
    QCheckBox* cinemaLfeFilterBypassButton_{};
    QLabel* bassStatus_{};
    QComboBox* bassProcessBox_{};
    QComboBox* bassFirBox_{};
    QComboBox* bassAlgorithmBox_{};
    QCheckBox* bassAllPassButton_{};
    QCheckBox* bassHighPassButton_{};
    QCheckBox* bassLowPassButton_{};
    QSlider* bassHighPassSlider_{};
    QSlider* bassLowPassSlider_{};
    QSlider* bassApFrequencySlider_{};
    QSlider* bassApQSlider_{};
    QLabel* delayStatus_{};
    QLabel* coarseDelayLabel_{};
    ui::DelayEditModel fineDelayEdit_, coarseDelayEdit_;
    struct PendingDelay { double value; bool failed; std::uint64_t afterSequence; };
    std::map<QString, PendingDelay> pendingDelays_;
    int fineDelayLastRaw_ = 0;
    std::array<QPushButton*,16> routingTiles_{};
    std::array<QFrame*,10> cinemaLaneFrames_{};
    std::array<QLabel*,10> cinemaLaneLabels_{};
    std::array<QLabel*,10> cinemaLaneValues_{};
    std::array<QSlider*,10> cinemaTrimSliders_{};
    std::array<QProgressBar*,10> cinemaMeters_{};
    std::vector<ComboBinding> comboBindings_; std::vector<ToggleBinding> toggleBindings_; std::vector<SliderBinding> sliderBindings_;
    std::vector<FilterPageBinding> filterPages_;
    EngineTelemetrySnapshot lastTelemetry_{};
    bool lastTelemetryValid_ = false;
    int requestedMonitorChannel_ = -2;
};
}

QWidget* createCrossoverView(QWidget* owner, bool openBandwidthProtector)
{
    return new CrossoverView(owner, openBandwidthProtector);
}
}
