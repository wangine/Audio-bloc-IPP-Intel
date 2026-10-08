#include "LiveUiRefresh.h"
#include "OutputMatrixPage.h"

#include "Core/EngineTelemetry.h"

#include <QAbstractItemView>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <array>
#include <bit>
#include <cmath>

namespace entropy::host
{
namespace
{
QString peakText(double value)
{
    if (!(value > 0.0)) return QStringLiteral("--");
    return QStringLiteral("%1 dBFS").arg(20.0 * std::log10(value), 0, 'f', 1);
}

class OutputMatrixView final : public QWidget
{
public:
    explicit OutputMatrixView(QWidget* parent) : QWidget(parent)
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(7);

        auto* title = new QLabel(QStringLiteral("OUTPUT MATRIX — ACTIVE CROSSOVER ROLES"));
        title->setProperty("role", "sectionTitle");
        root->addWidget(title);
        auto* explanation = new QLabel(QStringLiteral(
            "16 semantic Crossover roles → 16 host destination lanes.  "
            "A lane is not a DAC: its physical device/channel label is supplied by the Host."));
        explanation->setWordWrap(true);
        root->addWidget(explanation);

        auto* tools = new QHBoxLayout;
        autoId_ = new QPushButton(QStringLiteral("AUTO-ID OUTPUTS"));
        autoId_->setEnabled(false);
        autoId_->setToolTip(QStringLiteral(
            "Host Device Manager tool. It will identify a selected physical channel "
            "without changing Output Matrix routing."));
        clear_ = new QPushButton(QStringLiteral("CLEAR ALL"));
        tools->addWidget(autoId_);
        tools->addWidget(clear_);
        tools->addSpacing(18);
        tools->addWidget(new QLabel(QStringLiteral("NODE INSPECTOR")));
        laneDetail_ = new QLabel(QStringLiteral(
            "Single-click an active-role node to inspect it. Double-click exclusively changes routing."));
        laneDetail_->setMinimumWidth(560);
        laneDetail_->setStyleSheet(QStringLiteral(
            "background:#0c141b;border:1px solid #2d4353;padding:6px;color:#aebbc6;"));
        tools->addWidget(laneDetail_, 1);
        root->addLayout(tools);

        status_ = new QLabel(QStringLiteral("Waiting for OutputMatrix.dll telemetry"));
        status_->setStyleSheet(QStringLiteral(
            "background:#101820;border:1px solid #243847;border-radius:3px;"
            "padding:6px;color:#aebbc6;"));
        root->addWidget(status_);

        table_ = new QTableWidget(16, 16);
        table_->setSelectionMode(QAbstractItemView::NoSelection);
        table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        table_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        table_->horizontalHeader()->setDefaultSectionSize(88);
        table_->horizontalHeader()->setMinimumSectionSize(62);
        table_->verticalHeader()->setDefaultSectionSize(33);
        table_->verticalHeader()->setMinimumWidth(190);
        table_->setAlternatingRowColors(false);
        for (int source = 0; source < 16; ++source)
        {
            table_->setVerticalHeaderItem(source,
                new QTableWidgetItem(QStringLiteral("ROLE %1").arg(source + 1, 2, 10,
                                                                   QLatin1Char('0'))));
            for (int destination = 0; destination < 16; ++destination)
            {
                auto* item = new QTableWidgetItem(QStringLiteral("·"));
                item->setTextAlignment(Qt::AlignCenter);
                item->setFlags(Qt::ItemIsEnabled);
                table_->setItem(source, destination, item);
            }
        }
        root->addWidget(table_, 1);

        auto* legend = new QLabel(QStringLiteral(
            "Single click = inspect only  |  Double click = set/clear ROLE → DESTINATION_LANE  |  "
            "Pulsing red/blue = inspected node  |  Green = route  |  "
            "Amber = intentional FP64 summing  |  Red role = ORPHANED  |  "
            "OFFLINE is reserved for a valid Host device assignment whose DAC is unavailable.  "
            "No gain, wheel, SRC, headroom or device control exists in this DLL."));
        legend->setWordWrap(true);
        legend->setStyleSheet(QStringLiteral("color:#8e9ba6;padding:3px;"));
        root->addWidget(legend);

        connect(table_, &QTableWidget::cellClicked, this,
            [this](int source, int destination) {
                if (!available_ || source < 0 || destination < 0) return;
                if ((activeSourceMask_ & (1u << static_cast<unsigned>(source))) == 0)
                    return;
                selectedSource_ = source;
                selectedDestination_ = destination;
                selectionPhase_ = 0.0;
                refreshSelectedNode();
            });
        connect(table_, &QTableWidget::cellDoubleClicked, this,
            [this](int source, int destination) {
                if (!available_ || source < 0 || destination < 0) return;
                if ((activeSourceMask_ & (1u << static_cast<unsigned>(source))) == 0)
                    return;
                selectedSource_ = source;
                selectedDestination_ = destination;
                const auto bit = 1u << static_cast<unsigned>(destination);
                const bool enable = (routeMasks_[static_cast<std::size_t>(source)] & bit) == 0;
                std::uint64_t sequence = 0;
                if (!writeEngineOutputMatrixRoute(static_cast<std::uint32_t>(source),
                        static_cast<std::uint32_t>(destination), enable, sequence))
                    status_->setText(QStringLiteral("Engine IPC unavailable — route unchanged"));
            });
        connect(clear_, &QPushButton::clicked, this, [this] {
            std::uint64_t sequence = 0;
            if (!writeEngineOutputMatrixClearAll(sequence))
                status_->setText(QStringLiteral("Engine IPC unavailable — routes unchanged"));
        });

        auto* timer = new QTimer(this);
        timer->setInterval(100);
        connect(timer, &QTimer::timeout, this, [this] { if (liveUiRefreshAllowed(this)) refresh(); });
        timer->start();
        refresh();
    }

private:
    void refreshSelectedNode()
    {
        if (selectedSource_ < 0 || selectedSource_ >= 16 ||
            selectedDestination_ < 0 || selectedDestination_ >= 16)
            return;
        const auto source = static_cast<std::size_t>(selectedSource_);
        const auto destination = static_cast<std::size_t>(selectedDestination_);
        const auto status = laneStatus_[destination];
        const QString state = status == EngineOutputLaneOnline ? QStringLiteral("ONLINE") :
            status == EngineOutputLaneOffline ? QStringLiteral("OFFLINE") :
                                                QStringLiteral("UNASSIGNED");
        const bool routed = (routeMasks_[source] & (1u << selectedDestination_)) != 0;
        const bool summing = (summingDestinationMask_ & (1u << selectedDestination_)) != 0;
        const bool orphaned = (orphanedSourceMask_ & (1u << selectedSource_)) != 0;
        QString laneLabel = laneLabels_[destination];
        if (laneLabel.isEmpty()) laneLabel = QStringLiteral("no physical DAC mapping");
        laneDetail_->setText(QStringLiteral("%1 [%2]  →  DEST_%3 [%4]  •  %5  •  %6  •  peak %7%8%9")
            .arg(roleLabels_[source], roleIds_[source])
            .arg(selectedDestination_ + 1, 2, 10, QLatin1Char('0'))
            .arg(laneLabel, routed ? QStringLiteral("ROUTED") : QStringLiteral("NOT ROUTED"),
                 state, peakText(peaks_[destination]),
                 summing ? QStringLiteral("  •  FP64 SUMMING") : QString{},
                 orphaned ? QStringLiteral("  •  ORPHANED ROLE") : QString{}));
        const QString colour = orphaned || status == EngineOutputLaneOffline
            ? QStringLiteral("#ff7070")
            : routed ? QStringLiteral("#55d98b") : QStringLiteral("#aebbc6");
        laneDetail_->setStyleSheet(QStringLiteral(
            "background:#0c141b;border:1px solid %1;padding:6px;color:%1;font-weight:700;")
            .arg(colour));
    }

    void refresh()
    {
        EngineTelemetrySnapshot telemetry{};
        if (!readEngineTelemetry(telemetry) || telemetry.outputMatrixAvailable == 0)
        {
            available_ = false;
            clear_->setEnabled(false);
            return;
        }
        available_ = true;
        activeSourceMask_ = telemetry.outputMatrixActiveSourceMask;
        orphanedSourceMask_ = telemetry.outputMatrixOrphanedSourceMask;
        summingDestinationMask_ = telemetry.outputMatrixSummingDestinationMask;
        clear_->setEnabled(true);
        for (int destination = 0; destination < 16; ++destination)
        {
            const auto index = static_cast<std::size_t>(destination);
            laneLabels_[index] = QString::fromUtf8(
                telemetry.outputMatrixLaneLabel[destination]);
            laneStatus_[index] = telemetry.outputMatrixLaneStatus[destination];
            peaks_[index] = telemetry.outputMatrixDestinationPeaks[destination];
            auto* header = table_->horizontalHeaderItem(destination);
            if (header == nullptr)
            {
                header = new QTableWidgetItem;
                table_->setHorizontalHeaderItem(destination, header);
            }
            header->setText(QStringLiteral("D%1\n%2")
                .arg(destination + 1, 2, 10, QLatin1Char('0'))
                .arg(laneStatus_[index] == EngineOutputLaneOffline
                         ? QStringLiteral("OFFLINE")
                         : laneStatus_[index] == EngineOutputLaneOnline
                               ? QStringLiteral("ONLINE")
                               : QStringLiteral("UNASSIGNED")));
            header->setToolTip(laneLabels_[index]);
        }

        for (int source = 0; source < 16; ++source)
        {
            const auto sourceIndex = static_cast<std::size_t>(source);
            routeMasks_[sourceIndex] = telemetry.outputMatrixSourceDestinationMask[source];
            const bool active = (telemetry.outputMatrixActiveSourceMask & (1u << source)) != 0;
            const bool orphaned = (telemetry.outputMatrixOrphanedSourceMask & (1u << source)) != 0;
            table_->setRowHidden(source, !active);
            QString label = QString::fromUtf8(telemetry.outputMatrixRoleLabel[source]);
            if (label.isEmpty()) label = QStringLiteral("ROLE %1").arg(source + 1);
            roleLabels_[sourceIndex] = label;
            roleIds_[sourceIndex] = QString::fromUtf8(telemetry.outputMatrixRoleId[source]);
            auto* rowHeader = table_->verticalHeaderItem(source);
            rowHeader->setText(QStringLiteral("%1%2")
                .arg(label, orphaned ? QStringLiteral("  • ORPHANED") : QString{}));
            rowHeader->setToolTip(QString::fromUtf8(telemetry.outputMatrixRoleId[source]));
            rowHeader->setForeground(orphaned ? QColor(QStringLiteral("#ff7070")) :
                active ? QColor(QStringLiteral("#e8edf1")) : QColor(QStringLiteral("#64717b")));
            for (int destination = 0; destination < 16; ++destination)
            {
                auto* item = table_->item(source, destination);
                const bool routed = (routeMasks_[sourceIndex] & (1u << destination)) != 0;
                const bool summing = (telemetry.outputMatrixSummingDestinationMask &
                                      (1u << destination)) != 0;
                const bool selected = source == selectedSource_ &&
                                      destination == selectedDestination_;
                if (selected && active)
                {
                    const double blend = (std::sin(selectionPhase_) + 1.0) * 0.5;
                    const auto mix = [blend](int red, int blue) {
                        return qRound(red + (blue - red) * blend);
                    };
                    item->setText(routed ? QStringLiteral("●") : QStringLiteral("○"));
                    item->setForeground(QColor(QStringLiteral("#ffffff")));
                    item->setBackground(QColor(mix(232, 48), mix(62, 142), mix(79, 255)));
                }
                else
                {
                    item->setText(routed ? QStringLiteral("●") : QStringLiteral("·"));
                    item->setForeground(routed ? QColor(QStringLiteral("#071117")) :
                                                 QColor(QStringLiteral("#4f5d67")));
                    item->setBackground(routed
                        ? QColor(summing ? QStringLiteral("#e5a83c") : QStringLiteral("#29c77d"))
                        : QColor(active ? QStringLiteral("#101820") : QStringLiteral("#0b1116")));
                }
                item->setToolTip(QStringLiteral("%1 → DEST_%2%3\nSingle click: inspect only. Double click: %4 route.")
                    .arg(label)
                    .arg(destination + 1, 2, 10, QLatin1Char('0'))
                    .arg(routed && summing ? QStringLiteral(" — FP64 SUMMING") : QString{})
                    .arg(routed ? QStringLiteral("clear") : QStringLiteral("set")));
            }
        }
        selectionPhase_ += 0.32;
        if (selectionPhase_ > 6.283185307179586) selectionPhase_ -= 6.283185307179586;
        status_->setText(QStringLiteral(
            "Generation %1  •  %2 routes  •  %3 active roles  •  %4 summing lanes  •  %5 orphaned roles")
            .arg(telemetry.outputMatrixGeneration)
            .arg(telemetry.outputMatrixRouteCount)
            .arg(std::popcount(telemetry.outputMatrixActiveSourceMask))
            .arg(std::popcount(telemetry.outputMatrixSummingDestinationMask))
            .arg(std::popcount(telemetry.outputMatrixOrphanedSourceMask)));
        refreshSelectedNode();
    }

    QTableWidget* table_{};
    QLabel* laneDetail_{};
    QLabel* status_{};
    QPushButton* autoId_{};
    QPushButton* clear_{};
    bool available_ = false;
    int selectedSource_ = -1;
    int selectedDestination_ = -1;
    double selectionPhase_ = 0.0;
    std::uint32_t activeSourceMask_ = 0;
    std::uint32_t orphanedSourceMask_ = 0;
    std::uint32_t summingDestinationMask_ = 0;
    std::array<std::uint32_t, 16> routeMasks_{};
    std::array<std::uint32_t, 16> laneStatus_{};
    std::array<double, 16> peaks_{};
    std::array<QString, 16> laneLabels_{};
    std::array<QString, 16> roleLabels_{};
    std::array<QString, 16> roleIds_{};
};
} // namespace

QWidget* createOutputMatrixView(QWidget* owner)
{
    return new OutputMatrixView(owner);
}
} // namespace entropy::host
