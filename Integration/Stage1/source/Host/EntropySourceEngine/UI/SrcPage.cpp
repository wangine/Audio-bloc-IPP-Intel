#include "SrcPage.h"
#include "LiveUiRefresh.h"
#include "SourceUiBridge.h"

#include "Core/EngineTelemetry.h"
#include "ModuleViewRegistry.h"

#include <array>
#include <cmath>

#include <QGridLayout>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

namespace entropy::host
{
namespace
{
std::uint32_t roleForInstance(const QString &instance)
{
    if (instance == QStringLiteral("src.aux"))
        return EngineSrcAux;
    if (instance == QStringLiteral("src.cinema"))
        return EngineSrcCinema;
    return EngineSrcTidal;
}

std::uint32_t muteBit(std::uint32_t role)
{
    if (role == EngineSrcAux)
        return EngineMuteAux;
    if (role == EngineSrcCinema)
        return EngineMuteCinema;
    return EngineMuteTidal;
}

QString sourceRoleLabel(const QString &instance)
{
    if (instance == QStringLiteral("src.tidal"))
        return QStringLiteral("MUSIC");
    if (instance == QStringLiteral("src.aux"))
        return QStringLiteral("AUX");
    if (instance == QStringLiteral("src.cinema"))
        return QStringLiteral("CINEMA");
    return instance.mid(4).toUpper();
}

QString rateText(std::uint32_t rate)
{
    return rate == 0 ? QStringLiteral("—") : QStringLiteral("%1 kHz").arg(rate / 1000.0, 0, 'f', 1);
}

QString peakText(double peak)
{
    if (!(peak > 1.0e-12))
        return QStringLiteral("−∞ dBFS");
    return QStringLiteral("%1 dBFS").arg(20.0 * std::log10(peak), 0, 'f', 1);
}

const EngineSrcEngineCapability *capabilityFor(const EngineTelemetrySnapshot &snapshot,
                                               std::uint32_t engine)
{
    for (std::uint32_t index = 0; index < snapshot.srcEngineCount; ++index)
        if (snapshot.srcEngines[index].engine == engine)
            return &snapshot.srcEngines[index];
    return nullptr;
}

} // namespace

QWidget *createSrcView(const ModuleViewContext &context, QWidget *owner)
{
    const auto role = roleForInstance(context.instanceId);
    auto *view = new QWidget(owner);
    view->setMinimumSize(360, 215);
    view->setMaximumHeight(245);
    auto *root = new QVBoxLayout(view);
    root->setContentsMargins(6, 4, 6, 4);
    root->setSpacing(4);

    auto *controls = new QGroupBox(
        QStringLiteral("%1 — SRC CONTROL").arg(sourceRoleLabel(context.instanceId)), view);
    auto *controlsLayout = new QGridLayout(controls);
    controlsLayout->setContentsMargins(6, 6, 6, 6);
    controlsLayout->setVerticalSpacing(2);
    controlsLayout->setColumnStretch(1, 1);
    controlsLayout->addWidget(new QLabel(QStringLiteral("ENGINE"), controls), 0, 0);
    auto *engine = new QLabel(role == EngineSrcCinema ? QStringLiteral("WDL-Sinc FP64") : QStringLiteral("r8brain"), controls);
    QComboBox* cinemaEngine=nullptr;
    if(role==EngineSrcCinema) {
        cinemaEngine=new QComboBox(controls);
        cinemaEngine->addItem(QStringLiteral("WDL-Sinc FP64 %1 taps").arg(ENTROPY_CINEMA_SINC_TAPS),2);
        cinemaEngine->addItem(QStringLiteral("r8brain (reference)"),0);
        cinemaEngine->setToolTip(QStringLiteral("CINEMA only. r8brain comparison: 44.1/48/88.2/96/176.4/192 kHz. Engine changes restart the entire CINEMA bus."));
        controlsLayout->addWidget(cinemaEngine,0,1);
        engine->hide();
        QObject::connect(cinemaEngine,&QComboBox::activated,view,[cinemaEngine](int index){writeCinemaSrcEngine(cinemaEngine->itemData(index).toUInt());});
    } else controlsLayout->addWidget(engine, 0, 1);
    auto *mute = new QPushButton(QStringLiteral("MUTE SOURCE"), controls);
    mute->setCheckable(true);
    mute->setProperty("positiveToggle", true);
    controlsLayout->addWidget(mute, 0, 2);
    auto *policy = new QLabel(sourceRoleLabel(context.instanceId) +
        (role == EngineSrcCinema ? QStringLiteral(" / WDL sinc / %1 taps / shared 8ch phase").arg(ENTROPY_CINEMA_SINC_TAPS) : QStringLiteral(" / independent r8brain SRC")), controls);
    policy->setWordWrap(true);
    policy->setStyleSheet(QStringLiteral("color:#8fa9b8;"));
    controlsLayout->addWidget(policy, 1, 0, 1, 3);
    root->addWidget(controls);

    auto *telemetry = new QGroupBox(QStringLiteral("LIVE TELEMETRY"), view);
    auto *telemetryLayout = new QGridLayout(telemetry);
    telemetryLayout->setContentsMargins(6, 6, 6, 6);
    telemetryLayout->setVerticalSpacing(2);
    std::array<QLabel *, 6> values{};
    const std::array<QString, 6> names{{QStringLiteral("STATE"), QStringLiteral("RATE CONTRACT"),
                                        QStringLiteral("CHANNELS"), QStringLiteral("LATENCY"),
                                        QStringLiteral("FRAMES"), QStringLiteral("PEAK")}};
    for (int index = 0; index < static_cast<int>(values.size()); ++index)
    {
        auto *name = new QLabel(names[static_cast<std::size_t>(index)], telemetry);
        name->setStyleSheet(QStringLiteral("color:#7892a1;"));
        values[static_cast<std::size_t>(index)] = new QLabel(QStringLiteral("—"), telemetry);
        values[static_cast<std::size_t>(index)]->setProperty("class", "sectionTitle");
        telemetryLayout->addWidget(name, index / 3 * 2, index % 3);
        telemetryLayout->addWidget(values[static_cast<std::size_t>(index)], index / 3 * 2 + 1,
                                   index % 3);
    }
    root->addWidget(telemetry);

    auto *commandState = new QLabel(QStringLiteral("Waiting for engine telemetry…"), view);
    commandState->setWordWrap(true);
    commandState->setAlignment(Qt::AlignCenter);
    commandState->setStyleSheet(QStringLiteral("color:#f0bd4c;"));
    root->addWidget(commandState);

    QObject::connect(mute, &QPushButton::clicked, view,
                     [role](bool checked)
                     {
                         EngineControlSnapshot control{};
                         std::uint32_t mask = readSourceControl(control) ? control.muteMask : 0u;
                         const auto bit = muteBit(role);
                         mask = checked ? (mask | bit) : (mask & ~bit);
                         writeSourceMuteMask(mask);
                     });
    auto *timer = new QTimer(view);
    timer->setInterval(100);
    QObject::connect(
        timer, &QTimer::timeout, view,
        [view, role, engine, cinemaEngine, policy, mute, values, commandState]
        {
            if (!liveUiRefreshAllowed(view))
                return;
            EngineTelemetrySnapshot snapshot{};
            if (!readSourceTelemetry(snapshot))
            {
                commandState->setText(QStringLiteral("ENGINE TELEMETRY UNAVAILABLE"));
                commandState->setStyleSheet(QStringLiteral("color:#ff6577;"));
                engine->setEnabled(false);
                mute->setEnabled(false);
                return;
            }
            const auto &input = snapshot.inputs[role];
            if(cinemaEngine) {
                const QSignalBlocker blocker(cinemaEngine);
                cinemaEngine->setCurrentIndex(cinemaEngine->findData(input.srcEngine));
                policy->setText(input.srcEngine==2 ? QStringLiteral("CINEMA / WDL sinc / %1 taps / shared 8ch phase").arg(ENTROPY_CINEMA_SINC_TAPS) : QStringLiteral("CINEMA / r8brain reference / synchronized 8ch"));
            }
            const bool muted = input.muted != 0;
            {
                const QSignalBlocker block(mute);
                mute->setChecked(muted);
                mute->setText(muted ? QStringLiteral("UNMUTE SOURCE")
                                    : QStringLiteral("MUTE SOURCE"));
            }
            mute->setEnabled(true);
            const bool selectedAvailable = role == EngineSrcCinema ? (input.srcEngine == 2 || input.srcEngine == 0) : capabilityFor(snapshot, input.srcEngine) != nullptr;
            uint32_t demandMask = 7;
            readSourceDemandMask(demandMask);
            const bool requested = (demandMask & muteBit(role)) != 0;
            values[0]->setText(!input.bridgeConnected ? QStringLiteral("OFFLINE")
                               : muted                ? QStringLiteral("MUTED")
                               : !requested           ? QStringLiteral("STANDBY / NOT REQUESTED")
                               : input.livePathPassed ? QStringLiteral("LIVE")
                                                      : QStringLiteral("WAITING"));
            values[0]->setStyleSheet(muted                  ? QStringLiteral("color:#ff9f55;")
                                     : requested && input.livePathPassed ? QStringLiteral("color:#43e878;")
                                                            : QStringLiteral("color:#f0bd4c;"));
            values[1]->setText(QStringLiteral("%1 → 192.0 kHz").arg(rateText(input.sourceRate)));
            values[2]->setText(QStringLiteral("%1 FP64").arg(input.channels));
            if (input.sourceRate == 192000)
                values[3]->setText(QStringLiteral("FP64 bypass / SRC 0 ms"));
            else if (role == EngineSrcCinema && input.srcEngine == 2)
                values[3]->setText(QStringLiteral("%1 samples / %2 ms")
                    .arg(input.srcLatencyFrames).arg(input.srcLatencyFrames / 192.0, 0, 'f', 3));
            else
                values[3]->setText(QStringLiteral("Not reported by r8brain"));
            values[3]->setToolTip(QStringLiteral("SRC filter latency only. Capture, FIFO and output latency are separate. r8brain's zero alignment report does not measure startup buffering."));
            values[4]->setText(QStringLiteral("%1 in / %2 out")
                                   .arg(input.capturedFrames)
                                   .arg(input.convertedFrames));
            values[5]->setText(peakText(input.endpointPeak));

            commandState->setText(selectedAvailable ? (role == EngineSrcCinema && input.srcEngine == 2 ? (input.sourceRate == 192000 ? QStringLiteral("FP64 BYPASS / SRC latency 0 ms") : QStringLiteral("WDL-Sinc FP64 / %1 taps / 192 kHz").arg(ENTROPY_CINEMA_SINC_TAPS)) : QStringLiteral("r8brain / FP64 / 192 kHz"))
                                                     : QStringLiteral("SRC UNAVAILABLE"));
            commandState->setStyleSheet(selectedAvailable ? QStringLiteral("color:#43e878;")
                                                          : QStringLiteral("color:#ff6577;"));
        });
    timer->start();
    return view;
}
} // namespace entropy::host
