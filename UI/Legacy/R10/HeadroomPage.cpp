#include "LiveUiRefresh.h"
#include "HeadroomPage.h"

#include "Core/EngineTelemetry.h"

#include <entropy/headroom_abi.h>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace entropy::host
{
namespace
{
double dbFromGain(double gain) noexcept
{
    return gain > 0.0 && std::isfinite(gain)
        ? -20.0 * std::log10(gain) : 300.0;
}

double dbfsFromPeak(double peak) noexcept
{
    return peak > 0.0 && std::isfinite(peak)
        ? 20.0 * std::log10(peak) : -160.0;
}

QString faultText(std::uint32_t flags)
{
    if (flags == 0) return QStringLiteral("NONE");
    QStringList faults;
    if (flags & ENTROPY_HEADROOM_FAULT_INVALID_CONFIG)
        faults << QStringLiteral("INVALID CONFIG");
    if (flags & ENTROPY_HEADROOM_FAULT_INVALID_LEDGER)
        faults << QStringLiteral("INVALID LEDGER");
    if (flags & ENTROPY_HEADROOM_FAULT_NONFINITE_AUDIO)
        faults << QStringLiteral("NON-FINITE AUDIO");
    return faults.join(QStringLiteral(" + "));
}

QString analyzerText(std::uint32_t state)
{
    switch (state)
    {
        case ENTROPY_HEADROOM_ANALYZER_IDLE:
            return QStringLiteral("IDLE");
        case ENTROPY_HEADROOM_ANALYZER_TRACKING:
            return QStringLiteral("TRACKING");
        case ENTROPY_HEADROOM_ANALYZER_HOLD:
            return QStringLiteral("HOLD");
        case ENTROPY_HEADROOM_ANALYZER_RECOVERY:
            return QStringLiteral("RELEASE");
        case ENTROPY_HEADROOM_ANALYZER_CAPACITY_EXCEEDED:
            return QStringLiteral("CAPACITY EXCEEDED");
        default:
            return QStringLiteral("UNKNOWN");
    }
}

QString limitingChannels(std::uint32_t mask)
{
    if (mask == 0) return QStringLiteral("none");
    QStringList result;
    for (std::uint32_t channel = 0; channel < 16; ++channel)
        if (mask & (1u << channel))
            result << QStringLiteral("CH%1").arg(channel + 1);
    return result.join(QStringLiteral(", "));
}

void setBadge(QLabel* label, const QString& text,
              const char* color, const char* background, const char* border)
{
    label->setText(text);
    label->setStyleSheet(QStringLiteral(
        "color:%1;background:%2;border:1px solid %3;"
        "border-radius:3px;padding:6px;font-weight:700;")
        .arg(QString::fromLatin1(color),
             QString::fromLatin1(background),
             QString::fromLatin1(border)));
}

class HeadroomView final : public QWidget
{
public:
    explicit HeadroomView(QWidget* parent) : QWidget(parent)
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(9, 7, 9, 7);
        root->setSpacing(8);

        auto* controlBox = new QGroupBox(QStringLiteral("HEADROOM MANAGER — GLOBAL 16ch"));
        auto* control = new QGridLayout(controlBox);
        control->setHorizontalSpacing(12);
        control->setVerticalSpacing(8);

        state_ = new QLabel(QStringLiteral("WAITING FOR ABI"));
        state_->setAlignment(Qt::AlignCenter);
        state_->setMinimumWidth(180);
        control->addWidget(state_, 0, 0, 1, 3);

        control->addWidget(new QLabel(QStringLiteral("MODE")), 1, 0);
        mode_ = new QComboBox;
        mode_->addItem(QStringLiteral("AUTO GLOBAL 16ch"), ENTROPY_HEADROOM_MODE_AUTO);
        mode_->addItem(QStringLiteral("MANUAL SAFE"), ENTROPY_HEADROOM_MODE_MANUAL_SAFE);
        mode_->addItem(QStringLiteral("BIT PERFECT"), ENTROPY_HEADROOM_MODE_BIT_PERFECT);
        mode_->setToolTip(QStringLiteral(
            "AUTO applies one coherent broadband FP64 gain to all 16 lanes. "
            "MANUAL applies the exact gain below. BIT PERFECT reaches unity copy."));
        control->addWidget(mode_, 1, 1);

        clearFault_ = new QPushButton(QStringLiteral("CLEAR FAULT"));
        clearFault_->setToolTip(QStringLiteral(
            "Explicit recovery for a genuine latched Headroom fault. "
            "A valid V2 configuration must not require this at startup."));
        clearFault_->setVisible(false);
        control->addWidget(clearFault_, 1, 2);

        control->addWidget(new QLabel(QStringLiteral("MANUAL GAIN / AUTO MAX")), 2, 0);
        manualGainDb_ = new QDoubleSpinBox;
        manualGainDb_->setRange(-60.0, 0.0);
        manualGainDb_->setDecimals(2);
        manualGainDb_->setSingleStep(0.5);
        manualGainDb_->setSuffix(QStringLiteral(" dB"));
        manualGainDb_->setKeyboardTracking(false);
        manualGainDb_->setToolTip(QStringLiteral(
            "MANUAL: exact common gain. AUTO: maximum attenuation authority only. "
            "AUTO normally applies only the attenuation actually required."));
        control->addWidget(manualGainDb_, 2, 1);

        control->addWidget(new QLabel(QStringLiteral("AUTO PREVENTIVE RESERVE")), 3, 0);
        reserveDb_ = new QDoubleSpinBox;
        reserveDb_->setRange(0.0, 24.0);
        reserveDb_->setDecimals(2);
        reserveDb_->setSingleStep(0.10);
        reserveDb_->setSuffix(QStringLiteral(" dB"));
        reserveDb_->setKeyboardTracking(false);
        reserveDb_->setToolTip(QStringLiteral(
            "Optional reserve below 0 dBFS. This is preventive margin, not a limiter."));
        control->addWidget(reserveDb_, 3, 1);

        control->addWidget(new QLabel(QStringLiteral("MODE / MANUAL TRANSITION")), 4, 0);
        transitionMs_ = new QDoubleSpinBox;
        transitionMs_->setRange(1.0, 10000.0);
        transitionMs_->setDecimals(1);
        transitionMs_->setSingleStep(1.0);
        transitionMs_->setSuffix(QStringLiteral(" ms"));
        transitionMs_->setKeyboardTracking(false);
        transitionMs_->setToolTip(QStringLiteral(
            "Gain ramp used for mode/manual changes. This is not look-ahead and "
            "does not add an audio analysis buffer."));
        control->addWidget(transitionMs_, 4, 1);

        control->setColumnStretch(1, 1);
        root->addWidget(controlBox);

        auto* timingBox = new QGroupBox(QStringLiteral("AUTO TIMING"));
        auto* timing = new QGridLayout(timingBox);
        timing->setHorizontalSpacing(12);
        timing->setVerticalSpacing(8);

        timing->addWidget(new QLabel(QStringLiteral("PREVENTIVE ATTACK")), 0, 0);
        attackSlider_ = new QSlider(Qt::Horizontal);
        attackSlider_->setRange(100, 2000);
        attackSlider_->setTracking(false);
        attackSlider_->setToolTip(QStringLiteral(
            "Controls only additional preventive attenuation. "
            "Required same-block sample safety is immediate and is never delayed by ATTACK."));
        attackMs_ = new QSpinBox;
        attackMs_->setRange(100, 2000);
        attackMs_->setSuffix(QStringLiteral(" ms"));
        attackMs_->setKeyboardTracking(false);
        attackMs_->setToolTip(attackSlider_->toolTip());
        timing->addWidget(attackSlider_, 0, 1);
        timing->addWidget(attackMs_, 0, 2);

        timing->addWidget(new QLabel(QStringLiteral("RELEASE")), 1, 0);
        releaseSlider_ = new QSlider(Qt::Horizontal);
        releaseSlider_->setRange(1000, 10000);
        releaseSlider_->setTracking(false);
        releaseSlider_->setToolTip(QStringLiteral(
            "Return toward unity after the fixed 1000 ms hold. "
            "Long release reduces audible pumping and preserves dynamics."));
        releaseMs_ = new QSpinBox;
        releaseMs_->setRange(1000, 10000);
        releaseMs_->setSuffix(QStringLiteral(" ms"));
        releaseMs_->setKeyboardTracking(false);
        releaseMs_->setToolTip(releaseSlider_->toolTip());
        timing->addWidget(releaseSlider_, 1, 1);
        timing->addWidget(releaseMs_, 1, 2);

        timingInfo_ = new QLabel(QStringLiteral(
            "Safety attack: immediate when the current block would exceed 0 dBFS  •  Hold: 1000 ms"));
        timingInfo_->setWordWrap(true);
        timing->addWidget(timingInfo_, 2, 0, 1, 3);
        timing->setColumnStretch(1, 1);
        root->addWidget(timingBox);

        auto* liveBox = new QGroupBox(QStringLiteral("LIVE SAFETY"));
        auto* live = new QGridLayout(liveBox);
        live->setHorizontalSpacing(12);
        live->setVerticalSpacing(7);

        live->addWidget(new QLabel(QStringLiteral("INPUT SAMPLE PEAK")), 0, 0);
        inputPeak_ = new QLabel(QStringLiteral("—"));
        live->addWidget(inputPeak_, 0, 1);

        live->addWidget(new QLabel(QStringLiteral("SAMPLE SAFETY REQUIRED")), 1, 0);
        required_ = new QLabel(QStringLiteral("—"));
        live->addWidget(required_, 1, 1);

        live->addWidget(new QLabel(QStringLiteral("AUTO TARGET")), 2, 0);
        target_ = new QLabel(QStringLiteral("—"));
        live->addWidget(target_, 2, 1);

        live->addWidget(new QLabel(QStringLiteral("APPLIED ATTENUATION")), 3, 0);
        applied_ = new QLabel(QStringLiteral("—"));
        live->addWidget(applied_, 3, 1);

        live->addWidget(new QLabel(QStringLiteral("ANALYZER")), 4, 0);
        analyzer_ = new QLabel(QStringLiteral("—"));
        live->addWidget(analyzer_, 4, 1);

        live->addWidget(new QLabel(QStringLiteral("AUTO CAPACITY")), 5, 0);
        capacity_ = new QLabel(QStringLiteral("—"));
        capacity_->setAlignment(Qt::AlignCenter);
        live->addWidget(capacity_, 5, 1);

        live->setColumnStretch(1, 1);
        root->addWidget(liveBox);

        summary_ = new QLabel(QStringLiteral("Waiting for EntropyHeadroomManager.dll telemetry"));
        summary_->setWordWrap(true);
        summary_->setStyleSheet(QStringLiteral(
            "background:#101820;border:1px solid #243847;border-radius:3px;"
            "padding:7px;color:#aebbc6;"));
        root->addWidget(summary_);
        root->addStretch(1);

        const auto applyPolicy = [this] {
            if (refreshing_ || !available_ || mode_->currentIndex() < 0) return;
            std::uint64_t sequence = 0;
            if (!writeEngineHeadroomOperatingPolicy(
                    mode_->currentData().toUInt(), manualGainDb_->value(), sequence))
                summary_->setText(QStringLiteral("Engine IPC unavailable — policy unchanged"));
        };

        connect(mode_, &QComboBox::currentIndexChanged, this,
                [applyPolicy](int) { applyPolicy(); });
        connect(manualGainDb_, &QDoubleSpinBox::valueChanged, this,
                [applyPolicy](double) { applyPolicy(); });

        connect(reserveDb_, &QDoubleSpinBox::editingFinished, this, [this] {
            if (refreshing_ || !available_) return;
            std::uint64_t sequence = 0;
            if (!writeEngineHeadroomPreventiveReserve(0, reserveDb_->value(), sequence))
                summary_->setText(QStringLiteral("Engine IPC unavailable — reserve unchanged"));
        });

        connect(transitionMs_, &QDoubleSpinBox::editingFinished, this, [this] {
            if (refreshing_ || !available_) return;
            std::uint64_t sequence = 0;
            if (!writeEngineHeadroomTransitionMs(transitionMs_->value(), sequence))
                summary_->setText(QStringLiteral("Engine IPC unavailable — transition unchanged"));
        });

        connect(attackSlider_, &QSlider::valueChanged, this, [this](int value) {
            if (refreshing_) return;
            const QSignalBlocker blocker(attackMs_);
            attackMs_->setValue(value);
        });
        connect(attackMs_, &QSpinBox::valueChanged, this, [this](int value) {
            if (refreshing_) return;
            const QSignalBlocker blocker(attackSlider_);
            attackSlider_->setValue(value);
        });
        connect(attackSlider_, &QSlider::sliderReleased, this, [this] {
            if (refreshing_ || !available_) return;
            std::uint64_t sequence = 0;
            if (!writeEngineHeadroomAutoAttackMs(attackSlider_->value(), sequence))
                summary_->setText(QStringLiteral("Engine IPC unavailable — attack unchanged"));
        });
        connect(attackMs_, &QSpinBox::editingFinished, this, [this] {
            if (refreshing_ || !available_) return;
            std::uint64_t sequence = 0;
            if (!writeEngineHeadroomAutoAttackMs(attackMs_->value(), sequence))
                summary_->setText(QStringLiteral("Engine IPC unavailable — attack unchanged"));
        });

        connect(releaseSlider_, &QSlider::valueChanged, this, [this](int value) {
            if (refreshing_) return;
            const QSignalBlocker blocker(releaseMs_);
            releaseMs_->setValue(value);
        });
        connect(releaseMs_, &QSpinBox::valueChanged, this, [this](int value) {
            if (refreshing_) return;
            const QSignalBlocker blocker(releaseSlider_);
            releaseSlider_->setValue(value);
        });
        connect(releaseSlider_, &QSlider::sliderReleased, this, [this] {
            if (refreshing_ || !available_) return;
            std::uint64_t sequence = 0;
            if (!writeEngineHeadroomAutoReleaseMs(releaseSlider_->value(), sequence))
                summary_->setText(QStringLiteral("Engine IPC unavailable — release unchanged"));
        });
        connect(releaseMs_, &QSpinBox::editingFinished, this, [this] {
            if (refreshing_ || !available_) return;
            std::uint64_t sequence = 0;
            if (!writeEngineHeadroomAutoReleaseMs(releaseMs_->value(), sequence))
                summary_->setText(QStringLiteral("Engine IPC unavailable — release unchanged"));
        });

        connect(clearFault_, &QPushButton::clicked, this, [this] {
            std::uint64_t sequence = 0;
            if (!writeEngineHeadroomClearFault(sequence))
                summary_->setText(QStringLiteral("Engine IPC unavailable — fault remains latched"));
        });

        auto* timer = new QTimer(this);
        timer->setInterval(100);
        connect(timer, &QTimer::timeout, this,
                [this] { if (liveUiRefreshAllowed(this)) refresh(); });
        timer->start();
        refresh();
    }

private:
    void setControlsEnabled(bool enabled)
    {
        mode_->setEnabled(enabled);
        manualGainDb_->setEnabled(enabled);
        transitionMs_->setEnabled(enabled);
        reserveDb_->setEnabled(enabled && mode_->currentData().toUInt() == ENTROPY_HEADROOM_MODE_AUTO);
        const bool autoTiming = enabled &&
            mode_->currentData().toUInt() == ENTROPY_HEADROOM_MODE_AUTO;
        attackSlider_->setEnabled(autoTiming);
        releaseSlider_->setEnabled(autoTiming);
        attackMs_->setEnabled(autoTiming);
        releaseMs_->setEnabled(autoTiming);
    }

    void refresh()
    {
        EngineTelemetrySnapshot telemetry{};
        if (!readEngineTelemetry(telemetry) || telemetry.headroomAvailable == 0)
        {
            available_ = false;
            setControlsEnabled(false);
            clearFault_->setVisible(false);
            setBadge(state_, QStringLiteral("ABI UNAVAILABLE"),
                     "#ff7070", "#241216", "#6a3038");
            inputPeak_->setText(QStringLiteral("—"));
            required_->setText(QStringLiteral("—"));
            target_->setText(QStringLiteral("—"));
            applied_->setText(QStringLiteral("—"));
            analyzer_->setText(QStringLiteral("—"));
            setBadge(capacity_, QStringLiteral("UNAVAILABLE"),
                     "#aebbc6", "#182028", "#394957");
            summary_->setText(QStringLiteral("Headroom telemetry unavailable"));
            return;
        }

        available_ = true;
        refreshing_ = true;

        {
            const QSignalBlocker blocker(mode_);
            mode_->setCurrentIndex(mode_->findData(telemetry.headroomRequestedMode));
        }
        {
            const QSignalBlocker blocker(manualGainDb_);
            if (!manualGainDb_->hasFocus())
                manualGainDb_->setValue(telemetry.headroomFixedAttenuationDb);
        }
        {
            const QSignalBlocker blocker(reserveDb_);
            if (!reserveDb_->hasFocus())
                reserveDb_->setValue(telemetry.headroomPreventiveReserveDb[0]);
        }
        {
            const QSignalBlocker blocker(transitionMs_);
            if (!transitionMs_->hasFocus())
                transitionMs_->setValue(telemetry.headroomTransitionMs);
        }
        {
            const int value = static_cast<int>(std::lround(telemetry.headroomAutoAttackMs));
            const QSignalBlocker spinBlocker(attackMs_);
            const QSignalBlocker sliderBlocker(attackSlider_);
            if (!attackMs_->hasFocus() && !attackSlider_->isSliderDown()) {
                attackMs_->setValue(value);
                attackSlider_->setValue(value);
            }
        }
        {
            const int value = static_cast<int>(std::lround(telemetry.headroomAutoReleaseMs));
            const QSignalBlocker spinBlocker(releaseMs_);
            const QSignalBlocker sliderBlocker(releaseSlider_);
            if (!releaseMs_->hasFocus() && !releaseSlider_->isSliderDown()) {
                releaseMs_->setValue(value);
                releaseSlider_->setValue(value);
            }
        }

        const bool fault = telemetry.headroomFaultFlags != 0;
        clearFault_->setVisible(fault);
        clearFault_->setEnabled(fault);

        if (fault)
            setBadge(state_, QStringLiteral("FAULT / SAFE GATE"),
                     "#ff7070", "#241216", "#6a3038");
        else
            setBadge(state_, QStringLiteral("HEADROOM OK"),
                     "#43e878", "#102018", "#245a3d");

        const double peakDb = dbfsFromPeak(telemetry.headroomMeasuredInputPeak);
        const double requiredDb = (std::max)(0.0, peakDb);
        const double targetDb = dbFromGain(telemetry.headroomTargetGain[0]);
        const double appliedDb = telemetry.headroomGainReductionDb[0];

        inputPeak_->setText(QStringLiteral("%1 dBFS")
            .arg(peakDb, 0, 'f', 2));
        required_->setText(QStringLiteral("%1 dB")
            .arg(requiredDb, 0, 'f', 2));
        target_->setText(QStringLiteral("%1 dB")
            .arg(targetDb, 0, 'f', 2));
        applied_->setText(QStringLiteral("%1 dB")
            .arg(appliedDb, 0, 'f', 2));
        analyzer_->setText(analyzerText(telemetry.headroomAnalyzerState));

        if (telemetry.headroomRequestedMode != ENTROPY_HEADROOM_MODE_AUTO)
            setBadge(capacity_, QStringLiteral("N/A"),
                     "#aebbc6", "#182028", "#394957");
        else if (telemetry.headroomAnalyzerState ==
                 ENTROPY_HEADROOM_ANALYZER_CAPACITY_EXCEEDED)
            setBadge(capacity_, QStringLiteral("EXCEEDED"),
                     "#ff7070", "#241216", "#6a3038");
        else
            setBadge(capacity_, QStringLiteral("OK"),
                     "#43e878", "#102018", "#245a3d");

        QString modeText;
        if (telemetry.headroomModeTransitionActive != 0)
            modeText = QStringLiteral("transition %1%")
                .arg(qRound(telemetry.headroomModeTransitionProgress * 100.0));
        else if (telemetry.headroomEffectiveMode == ENTROPY_HEADROOM_MODE_AUTO)
            modeText = QStringLiteral("AUTO");
        else if (telemetry.headroomEffectiveMode == ENTROPY_HEADROOM_MODE_MANUAL_SAFE)
            modeText = QStringLiteral("MANUAL");
        else
            modeText = QStringLiteral("BIT PERFECT");

        const double availableDb = (std::max)(0.0, -telemetry.headroomFixedAttenuationDb);
        summary_->setText(QStringLiteral(
            "%1  •  AUTO max %2 dB  •  reserve %3 dB  •  limiting %4  •  "
            "fault %5  •  non-finite %6  •  sample-peak safety; true-peak pending")
            .arg(modeText)
            .arg(availableDb, 0, 'f', 2)
            .arg(telemetry.headroomPreventiveReserveDb[0], 0, 'f', 2)
            .arg(limitingChannels(telemetry.headroomLimitingChannelMask))
            .arg(faultText(telemetry.headroomFaultFlags))
            .arg(telemetry.headroomNonfiniteSampleCount));

        setControlsEnabled(telemetry.headroomModeAvailable != 0);
        attackSlider_->setEnabled(attackSlider_->isEnabled() && telemetry.headroomTimingAvailable != 0);
        releaseSlider_->setEnabled(releaseSlider_->isEnabled() && telemetry.headroomTimingAvailable != 0);
        attackMs_->setEnabled(attackMs_->isEnabled() && telemetry.headroomTimingAvailable != 0);
        releaseMs_->setEnabled(releaseMs_->isEnabled() && telemetry.headroomTimingAvailable != 0);

        timingInfo_->setText(telemetry.headroomTimingAvailable
            ? QStringLiteral(
                "Immediate same-block sample safety  •  preventive attack %1 ms  •  "
                "hold 1000 ms  •  release %2 ms")
                .arg(telemetry.headroomAutoAttackMs, 0, 'f', 0)
                .arg(telemetry.headroomAutoReleaseMs, 0, 'f', 0)
            : QStringLiteral("Timing extension unavailable in loaded Headroom DLL"));

        refreshing_ = false;
    }

    bool available_ = false;
    bool refreshing_ = false;

    QLabel* state_ = nullptr;
    QComboBox* mode_ = nullptr;
    QDoubleSpinBox* manualGainDb_ = nullptr;
    QDoubleSpinBox* reserveDb_ = nullptr;
    QDoubleSpinBox* transitionMs_ = nullptr;
    QSlider* attackSlider_ = nullptr;
    QSlider* releaseSlider_ = nullptr;
    QSpinBox* attackMs_ = nullptr;
    QSpinBox* releaseMs_ = nullptr;
    QPushButton* clearFault_ = nullptr;
    QLabel* timingInfo_ = nullptr;

    QLabel* inputPeak_ = nullptr;
    QLabel* required_ = nullptr;
    QLabel* target_ = nullptr;
    QLabel* applied_ = nullptr;
    QLabel* analyzer_ = nullptr;
    QLabel* capacity_ = nullptr;
    QLabel* summary_ = nullptr;
};
}

QWidget* createHeadroomView(QWidget* owner)
{
    auto* scroll = new QScrollArea(owner);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(new HeadroomView(scroll));
    return scroll;
}
} // namespace entropy::host
