#include "LiveUiRefresh.h"
#include "HeadroomPage.h"

#include "Core/EngineTelemetry.h"

#include <entropy/headroom_abi.h>

#include <QAbstractItemView>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSlider>
#include <QScrollArea>
#include <QElapsedTimer>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <array>
#include <bit>
#include <cmath>

namespace entropy::host
{
namespace
{
QString gainText(double gain)
{
    if (!(gain > 0.0) || !std::isfinite(gain)) return QStringLiteral("GATED");
    const double decibels = 20.0 * std::log10(gain);
    return QStringLiteral("%1  /  %2 dB").arg(gain, 0, 'f', 6).arg(decibels, 0, 'f', 2);
}

QString groupState(std::uint32_t status)
{
    switch (status)
    {
        case ENTROPY_HEADROOM_GROUP_UNITY: return QStringLiteral("UNITY");
        case ENTROPY_HEADROOM_GROUP_REDUCED: return QStringLiteral("REDUCED");
        case ENTROPY_HEADROOM_GROUP_TRANSITION: return QStringLiteral("TRANSITION");
        case ENTROPY_HEADROOM_GROUP_FAULT: return QStringLiteral("FAULT");
        default: return QStringLiteral("UNKNOWN");
    }
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

class HeadroomView final : public QWidget
{
public:
    explicit HeadroomView(QWidget* parent) : QWidget(parent)
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(9, 7, 9, 7);
        root->setSpacing(7);

        auto* policy = new QGroupBox(QStringLiteral("HOST POLICY & MODULE STATE"));
        policy->setToolTip(QStringLiteral(
            "Output Matrix → one coherent broadband FP64 gain across all 16 DEST lanes. "
            "No per-band gain, clipping, EQ, SRC, dither or DAC control."));
        auto* policyLayout = new QGridLayout(policy);
        policyLayout->setHorizontalSpacing(10);
        policyLayout->setVerticalSpacing(7);
        state_ = new QLabel(QStringLiteral("WAITING FOR ABI"));
        state_->setMinimumWidth(170);
        state_->setAlignment(Qt::AlignCenter);
        policyLayout->addWidget(state_, 0, 0, 1, 3);
        policyLayout->addWidget(new QLabel(QStringLiteral("OPERATING POLICY")), 1, 0);
        mode_ = new QComboBox;
        mode_->addItem(QStringLiteral("MANUAL SAFE"),
                       ENTROPY_HEADROOM_MODE_MANUAL_SAFE);
        mode_->addItem(QStringLiteral("BIT PERFECT"),
                       ENTROPY_HEADROOM_MODE_BIT_PERFECT);
        mode_->addItem(QStringLiteral("AUTO GLOBAL 16ch"),
                       ENTROPY_HEADROOM_MODE_AUTO);
        mode_->setMinimumWidth(215);
        mode_->setToolTip(QStringLiteral(
            "AUTO analyzes the terminal 16-channel FP64 programme and applies one common gain. "
            "MANUAL applies the exact common gain below. BIT PERFECT reaches an exact unity copy "
            "path after transition. Guardian remains independent and downstream."));
        policyLayout->addWidget(mode_, 1, 1);
        policyLayout->addWidget(new QLabel(QStringLiteral("MANUAL GAIN / AUTO LIMIT")), 2, 0);
        fixedAttenuationDb_ = new QDoubleSpinBox;
        fixedAttenuationDb_->setRange(-60.0, 0.0);
        fixedAttenuationDb_->setDecimals(2);
        fixedAttenuationDb_->setSingleStep(0.5);
        fixedAttenuationDb_->setSuffix(QStringLiteral(" dB"));
        fixedAttenuationDb_->setKeyboardTracking(false);
        fixedAttenuationDb_->setToolTip(QStringLiteral(
            "Exact broadband gain applied identically to all 16 DEST lanes by "
            "HeadroomManager.dll. Presets such as 0, -2, -4 and -6 dB are valid."));
        policyLayout->addWidget(fixedAttenuationDb_, 2, 1);
        modeTelemetry_ = new QLabel(QStringLiteral("WAITING"));
        modeTelemetry_->setMinimumWidth(165);
        modeTelemetry_->setAlignment(Qt::AlignCenter);
        modeTelemetry_->setWordWrap(true);
        policyLayout->addWidget(modeTelemetry_, 3, 0, 1, 3);
        policyLayout->addWidget(new QLabel(QStringLiteral("COHERENT GROUPS (HOST)")), 4, 0);
        groupCount_ = new QSpinBox;
        groupCount_->setRange(1, 1);
        groupCount_->setEnabled(false);
        groupCount_->setKeyboardTracking(false);
        policyLayout->addWidget(groupCount_, 4, 1);
        policyLayout->addWidget(new QLabel(QStringLiteral("MANUAL / BYPASS TRANSITION")), 5, 0);
        transitionMs_ = new QDoubleSpinBox;
        transitionMs_->setRange(1.0, 10000.0);
        transitionMs_->setDecimals(1);
        transitionMs_->setSingleStep(1.0);
        transitionMs_->setSuffix(QStringLiteral(" ms"));
        transitionMs_->setKeyboardTracking(false);
        policyLayout->addWidget(transitionMs_, 5, 1);
        clearFault_ = new QPushButton(QStringLiteral("CLEAR FAULT"));
        clearFault_->setToolTip(QStringLiteral(
            "Explicitly rearms the safe output gate after a valid ledger is present."));
        policyLayout->addWidget(clearFault_, 1, 2, 2, 1);
        policyLayout->setColumnStretch(1, 1);
        root->addWidget(policy);

        auto* timingBox=new QGroupBox(QStringLiteral("AUTO TIMING — DLL CONTROL"));
        auto* timingLayout=new QGridLayout(timingBox);
        for(int i=0;i<2;++i) {
            timingLayout->addWidget(new QLabel(i==0?"ATTACK":"RELEASE"),i,0);
            timingSliders_[i]=new QSlider(Qt::Horizontal);
            timingValues_[i]=new QSpinBox;
            const int lo=i==0?100:1000, hi=i==0?2000:10000;
            timingSliders_[i]->setRange(lo,hi);
            timingSliders_[i]->setTracking(false);
            timingSliders_[i]->setEnabled(false);
            timingValues_[i]->setEnabled(false);
            timingValues_[i]->setRange(lo,hi);
            timingValues_[i]->setSuffix(" ms");
            timingValues_[i]->setKeyboardTracking(false);
            timingSliders_[i]->setValue(i==0?500:4000);
            timingValues_[i]->setValue(i==0?500:4000);
            timingLayout->addWidget(timingSliders_[i],i,1);
            timingLayout->addWidget(timingValues_[i],i,2);
            connect(timingSliders_[i],&QSlider::valueChanged,this,[this,i](int value){
                if(refreshing_)return;
                const QSignalBlocker b(timingValues_[i]); timingValues_[i]->setValue(value);
                timingDirty_[i]=true;
            });
            connect(timingValues_[i],&QSpinBox::valueChanged,this,[this,i](int value){
                if(refreshing_)return;
                const QSignalBlocker b(timingSliders_[i]); timingSliders_[i]->setValue(value);
                timingDirty_[i]=true;
            });
        }
        timingStatus_=new QLabel("Waiting for timing ABI");
        timingStatus_->setWordWrap(true);
        timingLayout->addWidget(timingStatus_,2,0,1,3);
        root->addWidget(timingBox);

        summary_ = new QLabel(QStringLiteral("Waiting for EntropyHeadroomManager.dll telemetry"));
        summary_->setWordWrap(true);
        summary_->setStyleSheet(QStringLiteral(
            "background:#101820;border:1px solid #243847;border-radius:3px;"
            "padding:6px;color:#aebbc6;"));
        root->addWidget(summary_);

        auto* views = new QTabWidget;
        views->setToolTip(QStringLiteral(
            "All 16 Output Matrix destinations belong to one coherent terminal gain group. "
            "Qt only commands/observes; the DLL owns the gain."));
        auto* lanes = new QWidget;
        auto* lanesLayout = new QVBoxLayout(lanes);
        lanesLayout->setContentsMargins(3, 3, 3, 3);
        laneTable_ = new QTableWidget(16, 4);
        laneTable_->setHorizontalHeaderLabels({QStringLiteral("Lane"),
            QStringLiteral("Host label"), QStringLiteral("Signal state"),
            QStringLiteral("Coherent group")});
        laneTable_->verticalHeader()->hide();
        laneTable_->setSelectionMode(QAbstractItemView::NoSelection);
        laneTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        laneTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        laneTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        laneTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        laneTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        laneTable_->verticalHeader()->setDefaultSectionSize(31);
        for (int channel = 0; channel < 16; ++channel)
        {
            laneTable_->setItem(channel, 0, new QTableWidgetItem(
                QStringLiteral("DEST_%1").arg(channel + 1, 2, 10, QLatin1Char('0'))));
            laneTable_->setItem(channel, 1, new QTableWidgetItem(QStringLiteral("UNASSIGNED")));
            laneTable_->setItem(channel, 2, new QTableWidgetItem(QStringLiteral("IDLE")));
            auto* selector = new QComboBox;
            selectors_[static_cast<std::size_t>(channel)] = selector;
            laneTable_->setCellWidget(channel, 3, selector);
            connect(selector, &QComboBox::currentIndexChanged, this,
                [this, channel](int group) {
                    if (refreshing_ || !available_ || group < 0) return;
                    std::uint64_t sequence = 0;
                    if (!writeEngineHeadroomChannelGroup(
                            static_cast<std::uint32_t>(channel),
                            static_cast<std::uint32_t>(group), sequence))
                        summary_->setText(QStringLiteral(
                            "Engine IPC unavailable — coherent group unchanged"));
                });
        }
        lanesLayout->addWidget(laneTable_);
        views->addTab(lanes, QStringLiteral("GLOBAL 16ch"));

        auto* groups = new QWidget;
        auto* groupsLayout = new QVBoxLayout(groups);
        groupsLayout->setContentsMargins(3, 3, 3, 3);
        groupTable_ = new QTableWidget(16, 11);
        groupTable_->setHorizontalHeaderLabels({QStringLiteral("Group"),
            QStringLiteral("Lanes"), QStringLiteral("Derived +dB"),
            QStringLiteral("Guaranteed"), QStringLiteral("Reserve"),
            QStringLiteral("Required"), QStringLiteral("Current gain"),
            QStringLiteral("Target gain"), QStringLiteral("Reduction"),
            QStringLiteral("Margin"), QStringLiteral("State")});
        groupTable_->verticalHeader()->hide();
        groupTable_->setSelectionMode(QAbstractItemView::NoSelection);
        groupTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        groupTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        groupTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        for (int column = 2; column < 11; ++column)
            groupTable_->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
        groupTable_->verticalHeader()->setDefaultSectionSize(33);
        for (int group = 0; group < 16; ++group)
        {
            for (int column = 0; column < 11; ++column)
                if (column != 4)
                    groupTable_->setItem(group, column, new QTableWidgetItem);
            auto* reserve = new QDoubleSpinBox;
            reserve->setRange(0.0, 24.0);
            reserve->setDecimals(2);
            reserve->setSingleStep(0.10);
            reserve->setSuffix(QStringLiteral(" dB"));
            reserve->setKeyboardTracking(false);
            reserves_[static_cast<std::size_t>(group)] = reserve;
            groupTable_->setCellWidget(group, 4, reserve);
            connect(reserve, &QDoubleSpinBox::editingFinished, this,
                [this, group, reserve] {
                    if (refreshing_ || !available_) return;
                    std::uint64_t sequence = 0;
                    if (!writeEngineHeadroomPreventiveReserve(
                            static_cast<std::uint32_t>(group), reserve->value(), sequence))
                        summary_->setText(QStringLiteral(
                            "Engine IPC unavailable — preventive reserve unchanged"));
                });
        }
        groupsLayout->addWidget(groupTable_);
        views->addTab(groups, QStringLiteral("HEADROOM LEDGER"));
        root->addWidget(views, 1);

        connect(groupCount_, &QSpinBox::valueChanged, this, [this](int count) {
            if (refreshing_ || !available_) return;
            std::uint64_t sequence = 0;
            if (!writeEngineHeadroomGroupCount(static_cast<std::uint32_t>(count), sequence))
                summary_->setText(QStringLiteral("Engine IPC unavailable — group count unchanged"));
        });
        connect(transitionMs_, &QDoubleSpinBox::editingFinished, this, [this] {
            if (refreshing_ || !available_) return;
            std::uint64_t sequence = 0;
            if (!writeEngineHeadroomTransitionMs(transitionMs_->value(), sequence))
                summary_->setText(QStringLiteral("Engine IPC unavailable — transition unchanged"));
        });
        connect(clearFault_, &QPushButton::clicked, this, [this] {
            std::uint64_t sequence = 0;
            if (!writeEngineHeadroomClearFault(sequence))
                summary_->setText(QStringLiteral("Engine IPC unavailable — fault remains latched"));
        });
        const auto applyPolicy = [this] {
            if (refreshing_ || !available_ || mode_->currentIndex() < 0) return;
            std::uint64_t sequence = 0;
            const auto requested = mode_->currentData().toUInt();
            if (!writeEngineHeadroomOperatingPolicy(
                    requested, fixedAttenuationDb_->value(), sequence))
                summary_->setText(QStringLiteral(
                    "Engine IPC unavailable — operating policy unchanged"));
        };
        connect(mode_, &QComboBox::currentIndexChanged, this,
                [applyPolicy](int) { applyPolicy(); });
        connect(fixedAttenuationDb_, &QDoubleSpinBox::valueChanged,
                this, [applyPolicy](double) { applyPolicy(); });

        auto* timer = new QTimer(this);
        timer->setInterval(100);
        connect(timer, &QTimer::timeout, this, [this] { if (liveUiRefreshAllowed(this)) refresh(); });
        timer->start();
        refresh();
    }

private:
    void refresh()
    {
        EngineTelemetrySnapshot telemetry{};
        if (!readEngineTelemetry(telemetry) || telemetry.headroomAvailable == 0)
        {
            available_ = false;
            state_->setText(QStringLiteral("ABI UNAVAILABLE"));
            state_->setStyleSheet(QStringLiteral(
                "color:#ff7070;background:#241216;border:1px solid #6a3038;"
                "border-radius:3px;padding:6px;font-weight:700;"));
            groupCount_->setEnabled(false);
            transitionMs_->setEnabled(false);
            clearFault_->setEnabled(false);
            mode_->setEnabled(false);
            fixedAttenuationDb_->setEnabled(false);
            for(int i=0;i<2;++i){timingSliders_[i]->setEnabled(false);timingValues_[i]->setEnabled(false);}
            timingStatus_->setText("Timing telemetry unavailable");
            return;
        }

        available_ = true;
        refreshing_ = true;
        const auto groupCount = static_cast<int>(telemetry.headroomGroupCount);
        {
            const QSignalBlocker blocker(groupCount_);
            groupCount_->setValue(groupCount);
        }
        {
            const QSignalBlocker blocker(transitionMs_);
            transitionMs_->setValue(telemetry.headroomTransitionMs);
        }
        groupCount_->setEnabled(false);
        transitionMs_->setEnabled(true);
        clearFault_->setEnabled(telemetry.headroomFaultFlags != 0);
        mode_->setEnabled(telemetry.headroomModeAvailable != 0);
        fixedAttenuationDb_->setEnabled(telemetry.headroomModeAvailable != 0);
        {
            const QSignalBlocker blocker(mode_);
            mode_->setCurrentIndex(mode_->findData(telemetry.headroomRequestedMode));
        }
        {
            const QSignalBlocker blocker(fixedAttenuationDb_);
            fixedAttenuationDb_->setValue(telemetry.headroomFixedAttenuationDb);
        }

        QString modeText;
        if (telemetry.headroomModeAvailable == 0)
            modeText = QStringLiteral("MODE ABI UNAVAILABLE");
        else if (telemetry.headroomModeTransitionActive != 0)
        {
            const QString target = telemetry.headroomRequestedMode ==
                ENTROPY_HEADROOM_MODE_BIT_PERFECT ? QStringLiteral("BIT PERFECT") :
                telemetry.headroomRequestedMode ==
                ENTROPY_HEADROOM_MODE_MANUAL_SAFE
                    ? QStringLiteral("MANUAL %1 dB").arg(
                        telemetry.headroomFixedAttenuationDb, 0, 'f', 2)
                    : QStringLiteral("AUTO GLOBAL 16ch");
            modeText = QStringLiteral("TRANSITION → %1  /  %2%")
                .arg(target)
                .arg(qRound(telemetry.headroomModeTransitionProgress * 100.0));
        }
        else if (telemetry.headroomEffectiveMode == ENTROPY_HEADROOM_MODE_BIT_PERFECT)
            modeText = QStringLiteral("BIT PERFECT / COPY");
        else if (telemetry.headroomEffectiveMode ==
                 ENTROPY_HEADROOM_MODE_MANUAL_SAFE)
            modeText = QStringLiteral("MANUAL SAFE / %1 dB")
                .arg(telemetry.headroomFixedAttenuationDb, 0, 'f', 2);
        else
            modeText = QStringLiteral("AUTO GLOBAL 16ch");
        modeTelemetry_->setText(modeText);
        const bool unity = telemetry.headroomRequestedMode ==
                           ENTROPY_HEADROOM_MODE_BIT_PERFECT;
        const bool fixed = telemetry.headroomRequestedMode ==
                            ENTROPY_HEADROOM_MODE_MANUAL_SAFE;
        modeTelemetry_->setStyleSheet(unity
            ? QStringLiteral("color:#48d6ff;background:#10242a;border:1px solid #2a7480;"
                             "border-radius:3px;padding:6px;font-weight:700;")
            : fixed
            ? QStringLiteral("color:#f0bd4c;background:#241d10;border:1px solid #735b24;"
                             "border-radius:3px;padding:6px;font-weight:700;")
            : QStringLiteral("color:#43e878;background:#102018;border:1px solid #245a3d;"
                             "border-radius:3px;padding:6px;font-weight:700;"));

        const bool fault = telemetry.headroomFaultFlags != 0;
        state_->setText(fault ? QStringLiteral("FAULT / SAFE GATE")
                              : QStringLiteral("VALID LEDGER"));
        state_->setStyleSheet(fault
            ? QStringLiteral("color:#ff7070;background:#241216;border:1px solid #6a3038;"
                             "border-radius:3px;padding:6px;font-weight:700;")
            : QStringLiteral("color:#43e878;background:#102018;border:1px solid #245a3d;"
                             "border-radius:3px;padding:6px;font-weight:700;"));

        summary_->setText(QStringLiteral(
            "Revision %1 / applied %2  •  GLOBAL 16ch coherent  •  %3 Hz  •  "
            "%4 transition samples  •  final FP64 peak %5 dBFS  •  AUTO analysis -%6 dB  •  "
            "mode: %7  •  fault: %8  •  non-finite: %9")
            .arg(telemetry.headroomStateRevision)
            .arg(telemetry.headroomAppliedRevision)
            .arg(telemetry.headroomHostEngineRate)
            .arg(telemetry.headroomTransitionSamples)
            .arg(telemetry.headroomMeasuredInputPeak > 0.0
                ? 20.0 * std::log10(telemetry.headroomMeasuredInputPeak) : -160.0,
                0, 'f', 2)
            .arg(telemetry.headroomAutoAnalysisReductionDb, 0, 'f', 2)
            .arg(modeText)
            .arg(faultText(telemetry.headroomFaultFlags))
            .arg(telemetry.headroomNonfiniteSampleCount));

        for (int channel = 0; channel < 16; ++channel)
        {
            auto* selector = selectors_[static_cast<std::size_t>(channel)];
            if (selector->count() != groupCount)
            {
                const QSignalBlocker blocker(selector);
                selector->clear();
                for (int group = 0; group < groupCount; ++group)
                    selector->addItem(QStringLiteral("GROUP %1").arg(group + 1, 2, 10,
                                                                      QLatin1Char('0')));
            }
            {
                const QSignalBlocker blocker(selector);
                selector->setCurrentIndex(static_cast<int>(telemetry.headroomChannelGroup[channel]));
            }
            const bool active = (telemetry.headroomActiveChannelMask & (1u << channel)) != 0;
            laneTable_->item(channel, 1)->setText(
                QString::fromUtf8(telemetry.headroomLaneLabel[channel]));
            laneTable_->item(channel, 2)->setText(active ? QStringLiteral("GAIN CONTROLLED")
                                                         : QStringLiteral("BIT-EXACT PASS"));
            laneTable_->item(channel, 2)->setForeground(
                active ? QColor(QStringLiteral("#43e878")) : QColor(QStringLiteral("#64717b")));
            selector->setEnabled(false);
        }

        for (int group = 0; group < 16; ++group)
        {
            const bool visible = group < groupCount;
            groupTable_->setRowHidden(group, !visible);
            if (!visible) continue;
            QStringList lanes;
            for (int channel = 0; channel < 16; ++channel)
                if (telemetry.headroomChannelGroup[channel] == static_cast<std::uint32_t>(group))
                    lanes << QStringLiteral("%1").arg(channel + 1);
            groupTable_->item(group, 0)->setText(
                QStringLiteral("GROUP %1").arg(group + 1, 2, 10, QLatin1Char('0')));
            groupTable_->item(group, 1)->setText(lanes.join(QStringLiteral("  ")));
            groupTable_->item(group, 2)->setText(QStringLiteral("%1 dB")
                .arg(telemetry.headroomDeclaredPositiveDb[group], 0, 'f', 2));
            groupTable_->item(group, 3)->setText(QStringLiteral("%1 dB")
                .arg(telemetry.headroomGuaranteedInputDb[group], 0, 'f', 2));
            {
                const QSignalBlocker blocker(reserves_[static_cast<std::size_t>(group)]);
                reserves_[static_cast<std::size_t>(group)]->setValue(
                    telemetry.headroomPreventiveReserveDb[group]);
            }
            groupTable_->item(group, 5)->setText(QStringLiteral("%1 dB")
                .arg(telemetry.headroomRequiredDb[group], 0, 'f', 2));
            groupTable_->item(group, 6)->setText(gainText(telemetry.headroomCurrentGain[group]));
            groupTable_->item(group, 7)->setText(gainText(telemetry.headroomTargetGain[group]));
            groupTable_->item(group, 8)->setText(QStringLiteral("%1 dB")
                .arg(telemetry.headroomGainReductionDb[group], 0, 'f', 2));
            groupTable_->item(group, 9)->setText(QStringLiteral("%1 dB")
                .arg(telemetry.headroomReserveMarginDb[group], 0, 'f', 2));
            const auto status = telemetry.headroomGroupStatus[group];
            groupTable_->item(group, 10)->setText(groupState(status));
            groupTable_->item(group, 10)->setForeground(
                status == ENTROPY_HEADROOM_GROUP_FAULT ? QColor(QStringLiteral("#ff7070")) :
                status == ENTROPY_HEADROOM_GROUP_REDUCED ? QColor(QStringLiteral("#f0bd4c")) :
                status == ENTROPY_HEADROOM_GROUP_TRANSITION ? QColor(QStringLiteral("#48d6ff")) :
                                                             QColor(QStringLiteral("#43e878")));
        }
        const bool timingEnabled=telemetry.headroomTimingAvailable &&
            telemetry.headroomRequestedMode==ENTROPY_HEADROOM_MODE_AUTO;
        if(timingPending_ && (telemetry.headroomAppliedSequence>=timingPending_ || timingWait_.elapsed()>3000))
            timingPending_=0;
        for(int i=0;i<2;++i) {
            const double accepted=i==0?telemetry.headroomAutoAttackMs:telemetry.headroomAutoReleaseMs;
            if(timingDirty_[i] && !timingPending_ && accepted==timingValues_[i]->value())timingDirty_[i]=false;
            timingSliders_[i]->setEnabled(timingEnabled);
            timingValues_[i]->setEnabled(timingEnabled);
            if(!timingEnabled) timingDirty_[i]=false;
            if(!timingDirty_[i] && !timingPending_ && !timingSliders_[i]->isSliderDown() && !timingValues_[i]->hasFocus()) {
                const int value=static_cast<int>(i==0?telemetry.headroomAutoAttackMs:telemetry.headroomAutoReleaseMs);
                timingSliders_[i]->setValue(value); timingValues_[i]->setValue(value);
            }
        }
        if(timingEnabled && !timingPending_) for(int i=0;i<2;++i) if(timingDirty_[i]) {
            std::uint64_t sequence=0;
            const bool sent=i==0?writeEngineHeadroomAutoAttackMs(timingValues_[i]->value(),sequence):
                                writeEngineHeadroomAutoReleaseMs(timingValues_[i]->value(),sequence);
            if(sent){timingPending_=sequence;timingWait_.restart();}
            break;
        }
        timingStatus_->setText(!telemetry.headroomTimingAvailable?"Timing extension unavailable in this DLL":
            timingPending_?"Pending Engine / DLL acknowledgement":
            QString("Accepted: attack %1 ms / release %2 ms • hold 1000 ms").arg(telemetry.headroomAutoAttackMs).arg(telemetry.headroomAutoReleaseMs));
        refreshing_ = false;
    }

    QLabel* state_{};
    std::array<QSlider*,2> timingSliders_{};
    std::array<QSpinBox*,2> timingValues_{};
    std::array<bool,2> timingDirty_{};
    std::uint64_t timingPending_=0;
    QElapsedTimer timingWait_;
    QLabel* timingStatus_{};
    QLabel* summary_{};
    QSpinBox* groupCount_{};
    QDoubleSpinBox* transitionMs_{};
    QPushButton* clearFault_{};
    QComboBox* mode_{};
    QDoubleSpinBox* fixedAttenuationDb_{};
    QLabel* modeTelemetry_{};
    QTableWidget* laneTable_{};
    QTableWidget* groupTable_{};
    std::array<QComboBox*, 16> selectors_{};
    std::array<QDoubleSpinBox*, 16> reserves_{};
    bool available_ = false;
    bool refreshing_ = false;
};
} // namespace

QWidget* createHeadroomView(QWidget* owner)
{
    auto* scroll=new QScrollArea(owner);
    scroll->setWidgetResizable(true);
    scroll->setWidget(new HeadroomView(scroll));
    return scroll;
}
} // namespace entropy::host
