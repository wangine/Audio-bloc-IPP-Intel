#include "LiveUiRefresh.h"
#include "DeviceManagerPage.h"
#include "DeviceAssignmentPresentation.h"
#include "OutputStatusSemantics.h"


#include "Core/EngineTelemetry.h"
#include "Client/DeviceCache.h"


#include <entropy/device_manager_abi.h>
#include <entropy/dac_sink_abi.h>
#include <entropy/egress_clock_abi.h>
#include <entropy/final_guardian_abi.h>
#include <entropy/pcm24_abi.h>

#include <QAbstractItemView>
#include <QByteArray>
#include <QColor>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>

#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace entropy::host
{
namespace
{
constexpr const char* deviceManagerId = "audio.entropy.device-manager.windows";

QString bindingStateName(std::uint32_t state)
{
    switch (state)
    {
        case ENTROPY_DEVICE_VERIFIED: return QStringLiteral("VERIFIED");
        case ENTROPY_DEVICE_OFFLINE: return QStringLiteral("OFFLINE");
        case ENTROPY_DEVICE_AMBIGUOUS: return QStringLiteral("AMBIGUOUS");
        case ENTROPY_DEVICE_CHANGED: return QStringLiteral("CHANGED");
        default: return QStringLiteral("UNVERIFIED");
    }
}

QString stateColor(std::uint32_t state)
{
    switch (state)
    {
        case ENTROPY_DEVICE_VERIFIED: return QStringLiteral("#43e878");
        case ENTROPY_DEVICE_OFFLINE: return QStringLiteral("#738895");
        case ENTROPY_DEVICE_AMBIGUOUS: return QStringLiteral("#ffb84d");
        case ENTROPY_DEVICE_CHANGED: return QStringLiteral("#ff7070");
        default: return QStringLiteral("#59d6ff");
    }
}

QString evidenceKindName(std::uint32_t kind)
{
    switch (kind)
    {
        case ENTROPY_DEVICE_EVIDENCE_ENDPOINT_ID: return QStringLiteral("Endpoint ID");
        case ENTROPY_DEVICE_EVIDENCE_INTERFACE_PATH: return QStringLiteral("Interface path");
        case ENTROPY_DEVICE_EVIDENCE_CONTAINER_ID: return QStringLiteral("Container ID");
        case ENTROPY_DEVICE_EVIDENCE_INSTANCE_ID: return QStringLiteral("Physical instance");
        case ENTROPY_DEVICE_EVIDENCE_VID_PID: return QStringLiteral("VID / PID");
        case ENTROPY_DEVICE_EVIDENCE_SERIAL_NUMBER: return QStringLiteral("Hardware serial");
        case ENTROPY_DEVICE_EVIDENCE_HARDWARE_IDS: return QStringLiteral("Hardware IDs");
        case ENTROPY_DEVICE_EVIDENCE_FRIENDLY_NAME: return QStringLiteral("Friendly name");
        case ENTROPY_DEVICE_EVIDENCE_CHANNEL_CAPABILITY: return QStringLiteral("Channels");
        case ENTROPY_DEVICE_EVIDENCE_EXACT_FORMATS: return QStringLiteral("Exact formats");
        case ENTROPY_DEVICE_EVIDENCE_DRIVER_IDENTITY: return QStringLiteral("Driver");
        default: return QStringLiteral("Unknown");
    }
}

QString stabilityName(std::uint32_t stability)
{
    switch (stability)
    {
        case ENTROPY_DEVICE_EVIDENCE_HARDWARE_IMMUTABLE: return QStringLiteral("HARDWARE");
        case ENTROPY_DEVICE_EVIDENCE_DEVICE_STABLE: return QStringLiteral("DEVICE-STABLE");
        case ENTROPY_DEVICE_EVIDENCE_INSTALLATION_STABLE: return QStringLiteral("INSTALL-STABLE");
        case ENTROPY_DEVICE_EVIDENCE_PORT_BOUND: return QStringLiteral("PORT-BOUND");
        case ENTROPY_DEVICE_EVIDENCE_VOLATILE: return QStringLiteral("VOLATILE");
        default: return QStringLiteral("DESCRIPTIVE");
    }
}

const EntropyDeviceEvidence* evidence(const EntropyDeviceInfo& device,
                                      std::uint32_t kind)
{
    const auto count = (std::min)(device.evidence_count,
                                  ENTROPY_DEVICE_MAX_EVIDENCE);
    for (std::uint32_t index = 0; index < count; ++index)
        if (device.evidence[index].kind == kind &&
            device.evidence[index].value_utf8[0] != 0)
            return &device.evidence[index];
    return nullptr;
}

const EntropyDeviceEvidence* evidence(const EntropyDeviceBinding& binding,
                                      std::uint32_t kind)
{
    const auto count = (std::min)(binding.evidence_count,
                                  ENTROPY_DEVICE_MAX_EVIDENCE);
    for (std::uint32_t index = 0; index < count; ++index)
        if (binding.evidence[index].kind == kind &&
            binding.evidence[index].value_utf8[0] != 0)
            return &binding.evidence[index];
    return nullptr;
}

QString exactFormatSummary(const EntropyDeviceInfo& device)
{
    QStringList rates;
    QStringList channels;
    QStringList containers;
    const auto count = (std::min)(device.format_count, ENTROPY_DEVICE_MAX_FORMATS);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        const auto& format = device.formats[index];
        if (format.encoding != ENTROPY_DEVICE_PCM_INTEGER || format.valid_bits != 24)
            continue;
        rates.push_back(QString::number(format.sample_rate / 1000u));
        channels.push_back(QString::number(format.channels));
        containers.push_back(QString::number(format.container_bits));
    }
    rates.removeDuplicates();
    channels.removeDuplicates();
    containers.removeDuplicates();
    return rates.isEmpty() ? QStringLiteral("No exact PCM24 96/192 format")
        : QStringLiteral("%1 kHz  •  %2 ch  •  PCM24-in-%3")
            .arg(rates.join('/'), channels.join('/'), containers.join('/'));
}

class DeviceManagerView final : public QWidget
{
public:
    DeviceManagerView(QWidget* parent)
        : QWidget(parent)
    {
        buildUi();
        loadModule();
        loadBindings();
        loadOutputConfigs();
        refreshInventory();
    }

private:
    struct SlotWidgets
    {
        QLabel* state = nullptr;
        QLabel* assignment = nullptr;
        QLabel* detail = nullptr;
        QPushButton* approve = nullptr;
        QPushButton* clear = nullptr;
        QPushButton* configure = nullptr;
        QLabel* outputConfig = nullptr;
    };

    struct OutputConfig
    {
        bool enabled = false;
        std::uint32_t sampleRate = 0;
        std::uint32_t channels = 0;
        std::uint32_t outputFormat = ENTROPY_PCM24_IN_32_LE_LEFT_ALIGNED;
        std::array<std::uint32_t, engineMaximumDestinationLanes> lanes{};
        OutputConfig() { lanes.fill(engineDestinationUnassigned); }
    };

    struct InventoryItem
    {
        std::uint32_t managerIndex = 0;
        EntropyDeviceInfo info{};
    };

    static bool hasConfiguredPins(const OutputConfig& config) noexcept
    {
        if (config.sampleRate == 0 || config.channels == 0 ||
            config.channels > config.lanes.size())
            return false;
        for (std::uint32_t channel = 0; channel < config.channels; ++channel)
            if (config.lanes[channel] == engineDestinationUnassigned)
                return false;
        return true;
    }

    QString outputStateText(std::size_t slot, bool engineConfigured,
                            bool sinkRunning) const
    {
        const auto& config = outputConfigs_[slot];
        const auto state = ui::projectDacOutputPresentation(
            bindingPresent_[slot], hasConfiguredPins(config), config.enabled,
            engineConfigured, sinkRunning);
        return QString::fromLatin1(ui::dacOutputPresentationText(state).data())
            .replace(QStringLiteral(" | "), QStringLiteral(" • "));
    }

    void buildUi()
    {
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(7);

        auto* scroll = new QScrollArea;
        scroll->setObjectName(QStringLiteral("deviceManagerScroll"));
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto* content = new QWidget;
        auto* contentLayout = new QVBoxLayout(content);
        contentLayout->setContentsMargins(0, 0, 0, 0);
        contentLayout->setSpacing(7);
        scroll->setWidget(content);

        auto* header = new QGroupBox(QStringLiteral("DEVICE MANAGER — PERSISTENT DAC IDENTITY"));
        auto* headerLayout = new QHBoxLayout(header);
        auto* explanation = new QLabel(QStringLiteral(
            "Control-plane only: identifies and validates physical render endpoints.  "
            "It never opens a DAC, processes audio, changes SRC rate or arms the output gate."));
        explanation->setWordWrap(true);
        headerLayout->addWidget(explanation, 1);
        refresh_ = new QPushButton(QStringLiteral("REFRESH DEVICES"));
        headerLayout->addWidget(refresh_);
        contentLayout->addWidget(header);

        moduleState_ = new QLabel(QStringLiteral("● DEVICE MANAGER NOT LOADED"));
        moduleState_->setProperty("role", "sectionTitle");
        moduleState_->setStyleSheet(QStringLiteral(
            "background:#101820;border:1px solid #334a58;border-radius:4px;"
            "padding:7px;color:#ff7070;font-weight:700;"));
        contentLayout->addWidget(moduleState_);

        auto* bindingLayout = new QHBoxLayout;
        const std::array<QString, engineMaximumDacDomains> logicalIds{
            QStringLiteral("DAC_A"), QStringLiteral("DAC_B")};
        for (std::size_t index = 0; index < logicalIds.size(); ++index)
        {
            auto* card = new QGroupBox(logicalIds[index]);
            card->setStyleSheet(QStringLiteral(
                "QGroupBox{border-top:2px solid #35caff;}"
                "QGroupBox::title{color:#eaf4fb;}"));
            auto* layout = new QVBoxLayout(card);
            slots_[index].state = new QLabel(QStringLiteral("● UNVERIFIED"));
            slots_[index].state->setAlignment(Qt::AlignCenter);
            slots_[index].state->setStyleSheet(QStringLiteral(
                "color:#59d6ff;font-size:13px;font-weight:700;padding:5px;"));
            layout->addWidget(slots_[index].state);
            slots_[index].assignment = new QLabel(QStringLiteral("No approved physical endpoint"));
            slots_[index].assignment->setAlignment(Qt::AlignCenter);
            slots_[index].assignment->setWordWrap(true);
            layout->addWidget(slots_[index].assignment);
            slots_[index].detail = new QLabel(QStringLiteral(
                "Select one active endpoint below, then approve explicitly."));
            slots_[index].detail->setWordWrap(true);
            slots_[index].detail->setAlignment(Qt::AlignCenter);
            slots_[index].detail->setStyleSheet(QStringLiteral("color:#8da9bd;"));
            layout->addWidget(slots_[index].detail, 1);
            slots_[index].outputConfig = new QLabel(QStringLiteral(
                "LIVE OUTPUT: NOT CONFIGURED"));
            slots_[index].outputConfig->setAlignment(Qt::AlignCenter);
            slots_[index].outputConfig->setStyleSheet(QStringLiteral(
                "color:#7894a5;font-weight:700;"));
            layout->addWidget(slots_[index].outputConfig);
            auto* actions = new QHBoxLayout;
            slots_[index].approve = new QPushButton(
                QStringLiteral("APPROVE AS %1").arg(logicalIds[index]));
            slots_[index].approve->setObjectName(
                QStringLiteral("approveDac%1").arg(QChar('A' + static_cast<int>(index))));
            slots_[index].approve->setProperty("positiveToggle", true);
            slots_[index].clear = new QPushButton(QStringLiteral("CLEAR BINDING"));
            slots_[index].configure = new QPushButton(QStringLiteral("CONFIGURE OUTPUT"));
            slots_[index].configure->setObjectName(QStringLiteral("configureDac%1").arg(QChar('A'+int(index))));
            actions->addWidget(slots_[index].approve, 1);
            actions->addWidget(slots_[index].clear);
            layout->addLayout(actions);
            layout->addWidget(slots_[index].configure);
            bindingLayout->addWidget(card, 1);

            connect(slots_[index].approve, &QPushButton::clicked, this,
                    [this, index] { approveSelection(index); });
            connect(slots_[index].clear, &QPushButton::clicked, this,
                    [this, index] { clearBinding(index); });
            connect(slots_[index].configure, &QPushButton::clicked, this,
                    [this, index] { configureOutput(index); });
        }
        contentLayout->addLayout(bindingLayout);

        auto* inventoryGroup = new QGroupBox(QStringLiteral(
            "WINDOWS RENDER ENDPOINTS — ACTIVE AND RETAINED OFFLINE RECORDS"));
        auto* inventoryLayout = new QHBoxLayout(inventoryGroup);
        devices_ = new QTableWidget;
        devices_->setObjectName(QStringLiteral("deviceInventory"));
        devices_->setColumnCount(6);
        devices_->setHorizontalHeaderLabels({QStringLiteral("State"),
            QStringLiteral("Assignment"), QStringLiteral("Physical endpoint"),
            QStringLiteral("Channels"), QStringLiteral("Exact PCM24 policy"),
            QStringLiteral("Identity authority")});
        devices_->setSelectionBehavior(QAbstractItemView::SelectRows);
        devices_->setSelectionMode(QAbstractItemView::SingleSelection);
        devices_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        devices_->setAlternatingRowColors(true);
        devices_->verticalHeader()->hide();
        devices_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        devices_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        devices_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        devices_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        devices_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
        devices_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
        inventoryLayout->addWidget(devices_, 3);

        auto* evidencePanel = new QVBoxLayout;
        selectedState_ = new QLabel(QStringLiteral("SELECT AN ENDPOINT"));
        selectedState_->setProperty("role", "sectionTitle");
        selectedState_->setAlignment(Qt::AlignCenter);
        evidencePanel->addWidget(selectedState_);
        evidence_ = new QTableWidget;
        evidence_->setColumnCount(3);
        evidence_->setHorizontalHeaderLabels({QStringLiteral("Evidence"),
            QStringLiteral("Stability"), QStringLiteral("Value")});
        evidence_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        evidence_->setSelectionMode(QAbstractItemView::NoSelection);
        evidence_->setWordWrap(false);
        evidence_->verticalHeader()->hide();
        evidence_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        evidence_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        evidence_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        evidencePanel->addWidget(evidence_, 1);
        identityWarning_ = new QLabel(QStringLiteral(
            "No endpoint selected. A unique discovery is never an automatic DAC binding."));
        identityWarning_->setWordWrap(true);
        identityWarning_->setStyleSheet(QStringLiteral(
            "background:#151c21;border:1px solid #574c2d;border-radius:4px;"
            "padding:7px;color:#f0bd4c;"));
        evidencePanel->addWidget(identityWarning_);
        inventoryLayout->addLayout(evidencePanel, 2);
        contentLayout->addWidget(inventoryGroup, 1);
        root->addWidget(scroll, 1);

        auto* gateRow = new QHBoxLayout;
        gateState_ = new QLabel(QStringLiteral(
            "OUTPUT GATE: HOST CONTROLLED / CLOSED — only required VERIFIED bindings may participate in ARM"));
        gateState_->setAlignment(Qt::AlignCenter);
        gateState_->setStyleSheet(QStringLiteral(
            "background:#171114;border:1px solid #71343b;border-radius:4px;"
            "padding:8px;color:#ff8b8b;font-weight:700;"));
        gateRow->addWidget(gateState_, 1);
        arm_ = new QPushButton(QStringLiteral("ARM LIVE OUTPUT"));
        arm_->setObjectName(QStringLiteral("armLiveOutput"));
        arm_->setProperty("positiveToggle", true);
        disarm_ = new QPushButton(QStringLiteral("DISARM / CLOSE"));
        disarm_->setObjectName(QStringLiteral("disarmLiveOutput"));
        gateRow->addWidget(arm_);
        gateRow->addWidget(disarm_);
        root->addLayout(gateRow);

        connect(refresh_, &QPushButton::clicked, this,
                [this] { entropy::ui::requestDevices({{"action","devices"}}); refreshInventory(); });
        connect(devices_, &QTableWidget::itemSelectionChanged, this,
                [this] { showSelectedEvidence(); });
        connect(arm_, &QPushButton::clicked, this, [this] { armOutput(); });
        connect(disarm_, &QPushButton::clicked, this, [this] { disarmOutput(); });
        auto* timer = new QTimer(this);
        timer->setInterval(250);
        connect(timer, &QTimer::timeout, this, [this] { if (liveUiRefreshAllowed(this)) { refreshInventory(); updateGateTelemetry(); } });
        timer->start();
    }

    void loadModule()
    {
        clientAvailable_=true; moduleState_->setText("● AUDIOCORE IPC — CORE OWNS DEVICES / NO UI AUDIO");
    }

    void loadBindings()
    {
        auto* core=entropy::ui::EngineFacade::current();if(!core)return;auto confirmed=core->devices()["bindings"].toArray();
        for(unsigned i=0;i<2;++i){bindingPresent_[i]=i<unsigned(confirmed.size())&&!confirmed[int(i)].isNull();bindings_[i]=bindingPresent_[i]?entropy::ui::deviceBinding(confirmed[int(i)].toObject()):EntropyDeviceBinding{};}
    }

    void saveBinding(std::size_t slot)
    {
        (void)slot; // Core persists confirmed binding through profile Save/Load.
    }

    void loadOutputConfigs()
    {
        auto* core=entropy::ui::EngineFacade::current();if(!core)return;auto confirmed=core->devices()["configuration"].toArray();
        for(unsigned i=0;i<2;++i){if(i>=unsigned(confirmed.size()))continue;auto d=confirmed[int(i)].toObject();auto& c=outputConfigs_[i];c.enabled=d["enabled"].toBool();c.sampleRate=d["rate"].toInt();c.channels=d["channels"].toInt();c.outputFormat=d["format"].toInt();auto lanes=d["lanes"].toArray();c.lanes.fill(engineDestinationUnassigned);for(int n=0;n<lanes.size()&&n<16;++n)c.lanes[unsigned(n)]=uint32_t(lanes[n].toInt());}
    }

    void saveOutputConfig(std::size_t slot)
    {
        (void)slot; // Local draft is submitted by publishOutputConfiguration().
    }

    void refreshInventory()
    {
        const auto* previous=selectedItem(); const QString selectedEndpoint=previous?QString::fromUtf8(previous->info.endpoint_id_utf8):QString();
        inventory_.clear();auto* core=entropy::ui::EngineFacade::current();clientAvailable_=core&&core->connected();if(!core)return;
        auto devices=core->devices()["devices"].toArray();for(int i=0;i<devices.size();++i)inventory_.push_back({uint32_t(i),entropy::ui::deviceInfo(devices[i].toObject())});
        // Windows retains old endpoints ahead of connected DACs. Present active
        // devices first while preserving discovery order and endpoint identity.
        std::stable_partition(inventory_.begin(),inventory_.end(),[](const InventoryItem& item){return item.info.active!=0;});
        loadBindings();loadOutputConfigs();populateInventory(selectedEndpoint);updateBindings();
    }

    void populateInventory(const QString& selectedEndpoint)
    {
        const int verticalPosition=devices_->verticalScrollBar()->value();
        const int horizontalPosition=devices_->horizontalScrollBar()->value();
        const QSignalBlocker selectionSignals(devices_);
        devices_->clearContents();
        devices_->setRowCount(static_cast<int>(inventory_.size()));
        for (int row = 0; row < static_cast<int>(inventory_.size()); ++row)
        {
            const auto& item = inventory_[static_cast<std::size_t>(row)];
            const auto& device = item.info;
            const bool active = device.active != 0;
            const auto assignmentMask = ui::assignmentMaskForDevice(
                device, bindings_, bindingPresent_);
            auto* state = new QTableWidgetItem(active ? QStringLiteral("ONLINE")
                                                      : QStringLiteral("OFFLINE"));
            state->setData(Qt::UserRole, item.managerIndex);
            state->setForeground(QColor(active ? QStringLiteral("#43e878")
                                                : QStringLiteral("#738895")));
            auto assignmentText = QString::fromLatin1(
                ui::assignmentLabel(assignmentMask).data());
            const auto* interfacePath = evidence(
                device, ENTROPY_DEVICE_EVIDENCE_INTERFACE_PATH);
            const auto portToken = ui::shortPortToken(
                interfacePath != nullptr ? interfacePath->value_utf8 : nullptr);
            if (!portToken.empty())
                assignmentText += QStringLiteral("\nPORT %1")
                    .arg(QString::fromStdString(portToken));
            auto* assignment = new QTableWidgetItem(assignmentText);
            assignment->setToolTip(interfacePath != nullptr
                ? QString::fromUtf8(interfacePath->value_utf8) : QString());
            auto* name = new QTableWidgetItem(QString::fromUtf8(device.display_name_utf8));
            if (name->text().isEmpty()) name->setText(QStringLiteral("Unnamed retained endpoint"));
            name->setToolTip(name->text());
            auto* channels = new QTableWidgetItem(device.maximum_channels == 0
                ? QStringLiteral("—") : QString::number(device.maximum_channels));
            auto* formats = new QTableWidgetItem(exactFormatSummary(device));
            const auto* serial = evidence(device, ENTROPY_DEVICE_EVIDENCE_SERIAL_NUMBER);
            auto* authority = new QTableWidgetItem(serial != nullptr
                ? QStringLiteral("HARDWARE SERIAL")
                : QStringLiteral("NO SERIAL / PORT-BOUND"));
            authority->setForeground(QColor(serial != nullptr
                ? QStringLiteral("#43e878") : QStringLiteral("#f0bd4c")));
            devices_->setItem(row, 0, state);
            devices_->setItem(row, 1, assignment);
            devices_->setItem(row, 2, name);
            devices_->setItem(row, 3, channels);
            devices_->setItem(row, 4, formats);
            devices_->setItem(row, 5, authority);
            if (!active)
                for (int column = 0; column < devices_->columnCount(); ++column)
                    devices_->item(row, column)->setForeground(QColor("#627783"));
            if (assignmentMask != 0)
            {
                for (int column = 0; column < devices_->columnCount(); ++column)
                {
                    devices_->item(row, column)->setBackground(QColor("#2a160d"));
                    devices_->item(row, column)->setToolTip(
                        devices_->item(row, column)->toolTip().isEmpty()
                            ? assignmentText : devices_->item(row, column)->toolTip());
                }
                assignment->setForeground(QColor("#ff7a1a"));
                name->setForeground(QColor("#ffad66"));
            }
        }
        if (!inventory_.empty())
        {
            int selectedRow=0;
            for(int i=0;i<int(inventory_.size());++i)if(QString::fromUtf8(inventory_[i].info.endpoint_id_utf8)==selectedEndpoint){selectedRow=i;break;}
            devices_->selectRow(selectedRow);
            showSelectedEvidence();
        }
        else
        {
            evidence_->setRowCount(0);
            selectedState_->setText(QStringLiteral("NO RENDER ENDPOINTS DISCOVERED"));
        }
        // selectRow scrolls to the selected endpoint. A telemetry refresh must
        // not undo the user's independent browsing position in the inventory.
        devices_->verticalScrollBar()->setValue(verticalPosition);
        devices_->horizontalScrollBar()->setValue(horizontalPosition);
    }

    const InventoryItem* selectedItem() const
    {
        const auto row = devices_->currentRow();
        if (row < 0 || row >= static_cast<int>(inventory_.size())) return nullptr;
        return &inventory_[static_cast<std::size_t>(row)];
    }

    void showSelectedEvidence()
    {
        const auto* selected = selectedItem();
        if (selected == nullptr) return;
        const auto& device = selected->info;
        const auto* interfacePath = evidence(
            device, ENTROPY_DEVICE_EVIDENCE_INTERFACE_PATH);
        const auto portToken = ui::shortPortToken(
            interfacePath != nullptr ? interfacePath->value_utf8 : nullptr);
        selectedState_->setText(QStringLiteral("%1  •  %2%3")
            .arg(device.active != 0 ? QStringLiteral("ONLINE") : QStringLiteral("OFFLINE"),
                 QString::fromUtf8(device.display_name_utf8),
                 portToken.empty() ? QString() : QStringLiteral("  •  PORT %1")
                    .arg(QString::fromStdString(portToken))));
        const auto count = (std::min)(device.evidence_count,
                                      ENTROPY_DEVICE_MAX_EVIDENCE);
        evidence_->clearContents();
        evidence_->setRowCount(static_cast<int>(count));
        for (std::uint32_t index = 0; index < count; ++index)
        {
            const auto& item = device.evidence[index];
            evidence_->setItem(static_cast<int>(index), 0,
                new QTableWidgetItem(evidenceKindName(item.kind)));
            auto* stability = new QTableWidgetItem(stabilityName(item.stability));
            stability->setForeground(QColor(item.stability >= ENTROPY_DEVICE_EVIDENCE_DEVICE_STABLE
                ? QStringLiteral("#43e878") : item.stability == ENTROPY_DEVICE_EVIDENCE_PORT_BOUND
                    ? QStringLiteral("#f0bd4c") : QStringLiteral("#7fa0b4")));
            evidence_->setItem(static_cast<int>(index), 1, stability);
            auto* value = new QTableWidgetItem(QString::fromUtf8(item.value_utf8));
            value->setToolTip(value->text());
            evidence_->setItem(static_cast<int>(index), 2, value);
        }
        const auto* serial = evidence(device, ENTROPY_DEVICE_EVIDENCE_SERIAL_NUMBER);
        if (serial == nullptr)
        {
            identityWarning_->setText(QStringLiteral(
                "NO HARDWARE SERIAL: this endpoint can be approved at its current address, "
                "but identical devices become AMBIGUOUS/CHANGED after identity evidence moves. "
                "The Host will not guess."));
            identityWarning_->setStyleSheet(QStringLiteral(
                "background:#251d12;border:1px solid #8a642e;border-radius:4px;"
                "padding:7px;color:#ffc866;"));
        }
        else
        {
            identityWarning_->setText(QStringLiteral(
                "Hardware-stable serial evidence is available for port-independent verification."));
            identityWarning_->setStyleSheet(QStringLiteral(
                "background:#0b2b23;border:1px solid #2c7651;border-radius:4px;"
                "padding:7px;color:#75f19c;"));
        }
        for (std::size_t slot = 0; slot < slots_.size(); ++slot)
        {
            const bool canAssign = ui::canAssignDeviceToSlot(
                device, slot, bindings_, bindingPresent_);
            slots_[slot].approve->setEnabled(
                device.active != 0 && clientAvailable_ && canAssign);
            slots_[slot].approve->setToolTip(canAssign ? QString() : QStringLiteral(
                "This physical endpoint is already assigned to the other DAC role."));
        }
    }

    void approveSelection(std::size_t slot)
    {
        const auto* currentSelection = selectedItem();
        if (currentSelection == nullptr || currentSelection->info.active == 0 || !clientAvailable_) return;
        const auto selectionCopy=*currentSelection;
        const auto* selected=&selectionCopy; // Modal refresh cannot invalidate this approved identity.
        if (!ui::canAssignDeviceToSlot(
                selected->info, slot, bindings_, bindingPresent_))
        {
            QMessageBox::warning(this, QStringLiteral("Physical DAC already assigned"),
                QStringLiteral(
                    "This exact endpoint/fingerprint belongs to the other DAC role. "
                    "Clear or reassign that role first; one physical DAC cannot own two domains."));
            return;
        }
        const auto logical = QByteArray("DAC_") + char('A' + static_cast<int>(slot));
        const auto* selectedPath = evidence(
            selected->info, ENTROPY_DEVICE_EVIDENCE_INTERFACE_PATH);
        QString message;
        if (bindingPresent_[slot])
        {
            const auto* approvedPath = evidence(
                bindings_[slot], ENTROPY_DEVICE_EVIDENCE_INTERFACE_PATH);
            message = QStringLiteral(
                "Reassign %1 to this exact physical endpoint?\n\n"
                "CURRENT ENDPOINT\n%2\n%3\n\n"
                "SELECTED REPLACEMENT\n%4\n%5\n\n"
                "The previous port-bound approval will be replaced explicitly.")
                .arg(QString::fromLatin1(logical),
                     QString::fromUtf8(bindings_[slot].approved_display_name_utf8),
                     approvedPath != nullptr ? QString::fromUtf8(approvedPath->value_utf8)
                                             : QString::fromUtf8(bindings_[slot].approved_endpoint_id_utf8),
                     QString::fromUtf8(selected->info.display_name_utf8),
                     selectedPath != nullptr ? QString::fromUtf8(selectedPath->value_utf8)
                                             : QString::fromUtf8(selected->info.endpoint_id_utf8));
        }
        else
        {
            message = QStringLiteral(
                "Approve this exact physical endpoint as %1?\n\n%2\n%3\n\n"
                "This creates a persistent Host binding. DeviceManager will never substitute another device.")
                .arg(QString::fromLatin1(logical),
                     QString::fromUtf8(selected->info.display_name_utf8),
                     selectedPath != nullptr ? QString::fromUtf8(selectedPath->value_utf8)
                                             : QString::fromUtf8(selected->info.endpoint_id_utf8));
        }
        if (QMessageBox::question(this,
                bindingPresent_[slot] ? QStringLiteral("Reassign DAC identity")
                                      : QStringLiteral("Approve DAC identity"), message,
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
        auto revision=QString::number(QDateTime::currentMSecsSinceEpoch());
        entropy::ui::requestDevices({{"action","approve-binding"},{"slot",int(slot)},{"endpoint",QString::fromUtf8(selected->info.endpoint_id_utf8)},{"approvalRevision",revision}});
    }

    void clearBinding(std::size_t slot)
    {
        if(!bindingPresent_[slot])return;if(QMessageBox::question(this,"Clear DAC binding","Clear this approved physical DAC identity?",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        entropy::ui::requestDevices({{"action","clear-binding"},{"slot",int(slot)}});
    }

    void configureOutput(std::size_t slot)
    {
        EngineTelemetrySnapshot current{};
        if (readEngineTelemetry(current) && current.outputArmed != 0)
        {
            QMessageBox::warning(this, QStringLiteral("DISARM required"),
                QStringLiteral("Close the output gate before changing a physical DAC mapping."));
            return;
        }
        if (!resolvedValid_[slot])
        {
            QMessageBox::warning(this, QStringLiteral("Binding not VERIFIED"),
                QStringLiteral("DAC_%1 must resolve as VERIFIED before its hardware output can be configured.")
                    .arg(QChar('A' + static_cast<int>(slot))));
            return;
        }
        const auto& device = resolvedDevices_[slot];
        QDialog dialog(this);
        dialog.setWindowTitle(QStringLiteral("Configure DAC_%1 exact output")
            .arg(QChar('A' + static_cast<int>(slot))));
        dialog.resize(620, 620);
        auto* root = new QVBoxLayout(&dialog);
        auto* warning = new QLabel(QStringLiteral(
            "Explicit pin-to-pin mapping. No Windows order, default device, name or fallback is used. "
            "Every physical channel must receive exactly one DEST lane before ARM."));
        warning->setWordWrap(true);
        warning->setStyleSheet(QStringLiteral(
            "background:#251d12;border:1px solid #8a642e;padding:8px;color:#ffc866;"));
        root->addWidget(warning);
        auto* enabled = new QCheckBox(QStringLiteral("This DAC domain participates in ARM"));
        enabled->setChecked(outputConfigs_[slot].enabled);
        root->addWidget(enabled);
        auto* format = new QComboBox;
        const auto count = (std::min)(device.format_count, ENTROPY_DEVICE_MAX_FORMATS);
        for (std::uint32_t index = 0; index < count; ++index)
        {
            const auto& candidate = device.formats[index];
            if (candidate.encoding != ENTROPY_DEVICE_PCM_INTEGER ||
                candidate.valid_bits != 24 ||
                (candidate.sample_rate != 96000 && candidate.sample_rate != 192000) ||
                (candidate.container_bits != 24 && candidate.container_bits != 32)) continue;
            const auto packed = candidate.container_bits == 24
                ? ENTROPY_PCM24_PACKED_LE : ENTROPY_PCM24_IN_32_LE_LEFT_ALIGNED;
            const auto data = (static_cast<qulonglong>(candidate.sample_rate) << 32) |
                (static_cast<qulonglong>(candidate.channels) << 16) | packed;
            format->addItem(QStringLiteral("PCM24 / %1 kHz / %2 channels / %3-bit container")
                .arg(candidate.sample_rate / 1000u).arg(candidate.channels)
                .arg(candidate.container_bits), data);
            if (candidate.sample_rate == outputConfigs_[slot].sampleRate &&
                candidate.channels == outputConfigs_[slot].channels &&
                packed == outputConfigs_[slot].outputFormat)
                format->setCurrentIndex(format->count() - 1);
        }
        root->addWidget(new QLabel(QStringLiteral("EXACT DAC FORMAT")));
        root->addWidget(format);
        auto* mapping = new QTableWidget;
        mapping->setColumnCount(2);
        mapping->setHorizontalHeaderLabels({QStringLiteral("Physical DAC channel"),
                                             QStringLiteral("Destination lane")});
        mapping->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        mapping->verticalHeader()->hide();
        mapping->setEditTriggers(QAbstractItemView::NoEditTriggers);
        root->addWidget(mapping, 1);
        std::array<QComboBox*, engineMaximumDestinationLanes> selectors{};
        const auto rebuildRows = [&]
        {
            const auto data = format->currentData().toULongLong();
            const auto channels = static_cast<std::uint32_t>((data >> 16) & 0xffffu);
            mapping->setRowCount(static_cast<int>(channels));
            for (std::uint32_t channel = 0; channel < channels; ++channel)
            {
                mapping->setItem(static_cast<int>(channel), 0,
                    new QTableWidgetItem(QStringLiteral("CH %1").arg(channel + 1)));
                auto* selector = new QComboBox;
                selector->addItem(QStringLiteral("— SELECT EXPLICITLY —"),
                                  engineDestinationUnassigned);
                for (std::uint32_t lane = 0; lane < engineMaximumDestinationLanes; ++lane)
                    selector->addItem(QStringLiteral("DEST_%1").arg(lane + 1, 2, 10, QChar('0')), lane);
                const auto saved = outputConfigs_[slot].lanes[channel];
                const auto savedIndex = selector->findData(saved);
                if (savedIndex >= 0) selector->setCurrentIndex(savedIndex);
                mapping->setCellWidget(static_cast<int>(channel), 1, selector);
                selectors[channel] = selector;
            }
        };
        connect(format, &QComboBox::currentIndexChanged, &dialog,
                [&] { rebuildRows(); });
        rebuildRows();
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save |
                                              QDialogButtonBox::Cancel);
        root->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]
        {
            if (format->currentIndex() < 0)
            {
                QMessageBox::warning(&dialog, QStringLiteral("No exact format"),
                    QStringLiteral("This endpoint exposes no supported exact PCM24 96/192 format."));
                return;
            }
            const auto data = format->currentData().toULongLong();
            const auto channels = static_cast<std::uint32_t>((data >> 16) & 0xffffu);
            std::array<bool, engineMaximumDestinationLanes> used{};
            auto next = outputConfigs_[slot];
            next.enabled = enabled->isChecked();
            next.sampleRate = static_cast<std::uint32_t>(data >> 32);
            next.channels = channels;
            next.outputFormat = static_cast<std::uint32_t>(data & 0xffffu);
            next.lanes.fill(engineDestinationUnassigned);
            for (std::uint32_t channel = 0; channel < channels; ++channel)
            {
                const auto lane = selectors[channel]->currentData().toUInt();
                if (next.enabled && (lane >= engineMaximumDestinationLanes || used[lane]))
                {
                    QMessageBox::warning(&dialog, QStringLiteral("Invalid pin mapping"),
                        QStringLiteral("Assign one unique DEST lane to every physical DAC channel."));
                    return;
                }
                next.lanes[channel] = lane;
                if (lane < engineMaximumDestinationLanes) used[lane] = true;
            }
            outputConfigs_[slot] = next;
            saveOutputConfig(slot);
            dialog.accept();
            updateBindings();
            publishOutputConfiguration();
        });
        dialog.exec();
    }

    EngineDacDomainControl domainControlForSlot(std::size_t slot) const
    {
        EngineDacDomainControl target{};
        if (slot >= outputConfigs_.size()) return target;

        const auto& config = outputConfigs_[slot];
        target.enabled = config.enabled ? 1u : 0u;
        target.sampleRate = config.sampleRate;
        target.channels = config.channels;
        target.outputFormat = config.outputFormat;

        const auto fallbackLogical = std::string("DAC_") +
            static_cast<char>('A' + static_cast<int>(slot));
        const char* logical = fallbackLogical.c_str();
        if (bindingPresent_[slot] && bindings_[slot].logical_device_id[0] != 0)
            logical = bindings_[slot].logical_device_id;
        std::strncpy(target.logicalDeviceId, logical,
                     sizeof(target.logicalDeviceId) - 1);

        if (bindingPresent_[slot])
            target.approvalRevision = bindings_[slot].approval_revision;

        if (resolvedValid_[slot])
        {
            std::strncpy(target.verifiedEndpointIdUtf8,
                         resolvedDevices_[slot].endpoint_id_utf8,
                         sizeof(target.verifiedEndpointIdUtf8) - 1);
            std::strncpy(target.verifiedFingerprint,
                         resolvedDevices_[slot].composite_fingerprint,
                         sizeof(target.verifiedFingerprint) - 1);
        }

        for (std::uint32_t channel = 0;
             channel < config.channels && channel < engineMaximumDestinationLanes;
             ++channel)
            target.destinationLaneForChannel[channel] = config.lanes[channel];

        return target;
    }

    void publishOutputConfiguration()
    {
        std::array<EngineDacDomainControl, engineMaximumDacDomains> request{};
        for (std::size_t slot = 0; slot < outputConfigs_.size(); ++slot)
            request[slot] = domainControlForSlot(slot);

        std::uint64_t sequence = 0;
        writeEngineHardwareOutputConfiguration(request.data(), request.size(), sequence);
    }

    void armOutput()
    {
        std::array<EngineDacDomainControl, engineMaximumDacDomains> request{};
        std::array<bool, engineMaximumDestinationLanes> used{};
        QStringList summary;
        std::uint32_t enabledCount = 0;

        for (std::size_t slot = 0; slot < outputConfigs_.size(); ++slot)
        {
            const auto& config = outputConfigs_[slot];

            // Preserve the entire configured physical domain regardless of
            // whether it participates in this ARM. Output Matrix assignment is
            // configuration state; ARM only consumes domains with enabled=1.
            request[slot] = domainControlForSlot(slot);

            if (!config.enabled) continue;

            if (!resolvedValid_[slot])
            {
                QMessageBox::critical(this, QStringLiteral("ARM blocked"),
                    QStringLiteral("DAC_%1 is not VERIFIED. Output remains CLOSED.")
                        .arg(QChar('A' + static_cast<int>(slot))));
                return;
            }

            auto& target = request[slot];
            QStringList pins;
            for (std::uint32_t channel = 0; channel < config.channels; ++channel)
            {
                const auto lane = config.lanes[channel];
                if (lane >= engineMaximumDestinationLanes || used[lane])
                {
                    QMessageBox::critical(this, QStringLiteral("ARM blocked"),
                        QStringLiteral("DEST mapping is incomplete or duplicated across DAC domains."));
                    return;
                }
                used[lane] = true;
                // Already copied by domainControlForSlot(); repeat the exact
                // value here only to keep ARM validation and summary adjacent.
                target.destinationLaneForChannel[channel] = lane;
                pins.push_back(QStringLiteral("CH%1←DEST_%2")
                    .arg(channel + 1).arg(lane + 1, 2, 10, QChar('0')));
            }

            summary.push_back(QStringLiteral("DAC_%1: PCM24/%2 kHz, %3 ch\n%4")
                .arg(QChar('A' + static_cast<int>(slot)))
                .arg(config.sampleRate / 1000u).arg(config.channels)
                .arg(pins.join(QStringLiteral(", "))));
            ++enabledCount;
        }

        if (enabledCount == 0)
        {
            QMessageBox::warning(this, QStringLiteral("ARM blocked"),
                QStringLiteral("Configure and enable at least one VERIFIED DAC domain."));
            return;
        }

        if (QMessageBox::warning(this, QStringLiteral("ARM PHYSICAL OUTPUT"),
            QStringLiteral("Verify every amplifier/speaker connection before continuing.\n\n%1\n\n"
                           "Open the exact DAC endpoint(s) and ARM live audio now?")
                .arg(summary.join(QStringLiteral("\n\n"))),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;

        std::uint64_t sequence = 0;
        if (!writeEngineHardwareOutputArm(request.data(), request.size(), sequence))
            QMessageBox::critical(this, QStringLiteral("ARM unavailable"),
                QStringLiteral("Audio engine control channel is unavailable. Output remains CLOSED."));
    }

    void disarmOutput()
    {
        std::uint64_t sequence = 0;
        if (!writeEngineHardwareOutputDisarm(sequence))
            QMessageBox::warning(this, QStringLiteral("DISARM unavailable"),
                QStringLiteral("Audio engine is not running."));
    }

    void updateGateTelemetry()
    {
        EngineTelemetrySnapshot telemetry{};
        if (!readEngineTelemetry(telemetry))
        {
            gateState_->setText(QStringLiteral("OUTPUT GATE: CLOSED — AUDIO ENGINE OFFLINE"));
            arm_->setEnabled(false);
            return;
        }
        arm_->setEnabled(telemetry.outputArmed == 0);
        const auto sinkName = [](std::uint32_t state) {
            switch (state)
            {
                case ENTROPY_DAC_SINK_OPEN: return QStringLiteral("OPEN");
                case ENTROPY_DAC_SINK_RUNNING: return QStringLiteral("RUNNING");
                case ENTROPY_DAC_SINK_DEVICE_LOST: return QStringLiteral("DEVICE LOST");
                case ENTROPY_DAC_SINK_FAULTED: return QStringLiteral("FAULT");
                default: return QStringLiteral("CLOSED");
            }
        };
        const auto clockName = [](std::uint32_t state) {
            switch (state)
            {
                case ENTROPY_EGRESS_CLOCK_LOCKED: return QStringLiteral("LOCKED");
                case ENTROPY_EGRESS_CLOCK_STARVED: return QStringLiteral("STARVED");
                case ENTROPY_EGRESS_CLOCK_RESYNC: return QStringLiteral("RESYNC");
                case ENTROPY_EGRESS_CLOCK_FAULT: return QStringLiteral("FAULT");
                default: return QStringLiteral("PREROLL");
            }
        };
        for (std::size_t slot = 0; slot < slots_.size(); ++slot)
        {
            const auto& domain = telemetry.outputDomains[slot];
            if (domain.configured != 0)
            {
                slots_[slot].outputConfig->setText(QStringLiteral(
                    "%1\nLIVE: PCM24/%2 kHz • %3 ch\n"
                    "SINK %4 • CLOCK %5 • SUB %6 / HW %7\n"
                    "FIFO %8 • %9 ppm • XRUN %10 • BUF %11\n"
                    "GUARD %12 • interventions %13 • max %14 dBTP • trip %15 dBTP\n"
                    "Measured us: CLK %16 • GUA %17 • PCM %18 • SUBMIT %19 • owner wall minus wait %20\n"
                    "RT cycles %21 • deadline-miss %22")
                    .arg(outputStateText(slot, true,
                         domain.sinkState == ENTROPY_DAC_SINK_RUNNING))
                    .arg(domain.sampleRate / 1000u).arg(domain.channels)
                    .arg(sinkName(domain.sinkState), clockName(domain.clockState))
                    .arg(domain.submittedFrames)
                    .arg(domain.devicePositionFrames)
                    .arg(domain.fifoFillFrames, 0, 'f', 1)
                    .arg(domain.correctionPpm, 0, 'f', 2)
                    .arg(domain.xrunCount)
                    .arg(domain.bufferFrames)
                    .arg(domain.guardianState)
                    .arg(domain.guardianInterventionCount)
                    .arg(domain.guardianMaximumTruePeak > 0.0
                        ? 20.0 * std::log10(domain.guardianMaximumTruePeak) : -120.0, 0, 'f', 2)
                    .arg(domain.guardianTripTruePeak > 0.0
                        ? 20.0 * std::log10(domain.guardianTripTruePeak) : -120.0, 0, 'f', 2)
                    .arg(domain.egressClockUs, 0, 'f', 1)
                    .arg(domain.egressGuardianUs, 0, 'f', 1)
                    .arg(domain.egressPcmUs, 0, 'f', 1)
                    .arg(domain.egressSubmitUs, 0, 'f', 1)
                    .arg(domain.egressTotalUs, 0, 'f', 1)
                    .arg(qulonglong(domain.egressLastThreadCycles))
                    .arg(qulonglong(domain.egressDeadlineMisses)));
                const bool healthy = domain.sinkState == ENTROPY_DAC_SINK_RUNNING &&
                    (domain.clockState == ENTROPY_EGRESS_CLOCK_LOCKED ||
                     domain.clockState == ENTROPY_EGRESS_CLOCK_PREROLL) &&
                    domain.faultFlags == 0 && domain.xrunCount == 0 &&
                    domain.guardianFaultFlags == 0;
                slots_[slot].outputConfig->setStyleSheet(telemetry.outputArmed && telemetry.outputGateClosed
                    ? QStringLiteral("color:#f0bd4c;font-weight:700;") : healthy
                    ? QStringLiteral("color:#43e878;font-weight:700;")
                    : QStringLiteral("color:#ff7070;font-weight:700;"));
            }
            else
            {
                const auto& config = outputConfigs_[slot];
                auto text = outputStateText(slot, false, false);
                if (hasConfiguredPins(config))
                    text += QStringLiteral("\nCONFIG: PCM24/%1 kHz • %2 ch • EXPLICIT PINS")
                        .arg(config.sampleRate / 1000u).arg(config.channels);
                slots_[slot].outputConfig->setText(text);
                slots_[slot].outputConfig->setStyleSheet(QStringLiteral(
                    "color:#7894a5;font-weight:700;"));
            }
        }
        if (telemetry.outputArmed != 0)
        {
            gateState_->setText((telemetry.outputGateClosed ? QStringLiteral("OUTPUT GATE: ARM RETAINED / MUTE — %1") : QStringLiteral("OUTPUT GATE: ARMED / LIVE — %1"))
                .arg(QString::fromUtf8(telemetry.outputDetail)));
            gateState_->setStyleSheet(QStringLiteral(
                "background:#0b2b23;border:1px solid #2c7651;border-radius:4px;"
                "padding:8px;color:#43e878;font-weight:700;"));
        }
        else
        {
            gateState_->setText(QStringLiteral("OUTPUT GATE: CLOSED — %1")
                .arg(QString::fromUtf8(telemetry.outputDetail)));
            gateState_->setStyleSheet(QStringLiteral(
                "background:#171114;border:1px solid #71343b;border-radius:4px;"
                "padding:8px;color:#ff8b8b;font-weight:700;"));
        }
    }

    void updateBindings()
    {
        for (std::size_t slot = 0; slot < slots_.size(); ++slot)
        {
            auto& widgets = slots_[slot];
            widgets.approve->setText(bindingPresent_[slot]
                ? QStringLiteral("REASSIGN AS DAC_%1")
                    .arg(QChar('A' + static_cast<int>(slot)))
                : QStringLiteral("APPROVE AS DAC_%1")
                    .arg(QChar('A' + static_cast<int>(slot))));
            resolvedValid_[slot] = false;
            widgets.clear->setEnabled(bindingPresent_[slot]);
            widgets.configure->setEnabled(false);
            const auto& output = outputConfigs_[slot];
            auto outputText = outputStateText(slot, false, false);
            if (hasConfiguredPins(output))
                outputText += QStringLiteral("\nCONFIG: PCM24/%1 kHz • %2 ch • EXPLICIT PINS")
                    .arg(output.sampleRate / 1000u).arg(output.channels);
            widgets.outputConfig->setText(outputText);
            if (!bindingPresent_[slot])
            {
                widgets.state->setText(QStringLiteral("● UNVERIFIED"));
                widgets.state->setStyleSheet(QStringLiteral(
                    "color:#59d6ff;font-size:13px;font-weight:700;padding:5px;"));
                widgets.assignment->setText(QStringLiteral("No approved physical endpoint"));
                widgets.detail->setText(QStringLiteral(
                    "Select one active endpoint below, then approve explicitly."));
                continue;
            }
            EntropyDeviceBindingResolution resolution{};
            resolution.struct_size = sizeof(resolution);
            auto* core=entropy::ui::EngineFacade::current();auto confirmed=core?core->devices()["bindings"].toArray():QJsonArray{};auto b=slot<size_t(confirmed.size())?confirmed[int(slot)].toObject():QJsonObject{};resolution.state=uint32_t(b["state"].toInt());entropy::ui::copyDeviceText(resolution.resolved_endpoint_id_utf8,b["resolvedEndpoint"]);entropy::ui::copyDeviceText(resolution.detail_utf8,b["detail"]);const auto result=b.isEmpty()?ENTROPY_ERROR_INVALID_STATE:ENTROPY_OK;
            if (result != ENTROPY_OK)
            {
                resolution.state = ENTROPY_DEVICE_OFFLINE;
                std::strncpy(resolution.detail_utf8, "DeviceManager unavailable",
                             sizeof(resolution.detail_utf8) - 1);
            }
            const auto state = bindingStateName(resolution.state);
            widgets.state->setText(QStringLiteral("● %1").arg(state));
            widgets.state->setStyleSheet(QStringLiteral(
                "color:%1;font-size:13px;font-weight:700;padding:5px;")
                    .arg(stateColor(resolution.state)));
            widgets.assignment->setText(QStringLiteral("%1\nApproved revision %2")
                .arg(QString::fromUtf8(bindings_[slot].approved_display_name_utf8))
                .arg(bindings_[slot].approval_revision));
            widgets.detail->setText(QString::fromUtf8(resolution.detail_utf8));
            if (resolution.state == ENTROPY_DEVICE_VERIFIED)
            {
                const auto found = std::find_if(inventory_.begin(), inventory_.end(),
                    [&resolution](const InventoryItem& item) {
                        return std::strcmp(item.info.endpoint_id_utf8,resolution.resolved_endpoint_id_utf8)==0;
                    });
                if (found != inventory_.end())
                {
                    resolvedDevices_[slot] = found->info;
                    resolvedValid_[slot] = true;
                    widgets.configure->setEnabled(true);
                }
            }
        }
    }

    QLabel* moduleState_ = nullptr;
    QLabel* selectedState_ = nullptr;
    QLabel* identityWarning_ = nullptr;
    QLabel* gateState_ = nullptr;
    QPushButton* arm_ = nullptr;
    QPushButton* disarm_ = nullptr;
    QPushButton* refresh_ = nullptr;
    QTableWidget* devices_ = nullptr;
    QTableWidget* evidence_ = nullptr;
    std::array<SlotWidgets, engineMaximumDacDomains> slots_{};
    std::array<EntropyDeviceBinding, engineMaximumDacDomains> bindings_{};
    std::array<bool, engineMaximumDacDomains> bindingPresent_{};
    std::array<OutputConfig, engineMaximumDacDomains> outputConfigs_{};
    std::array<EntropyDeviceInfo, engineMaximumDacDomains> resolvedDevices_{};
    std::array<bool, engineMaximumDacDomains> resolvedValid_{};
    std::vector<InventoryItem> inventory_;
    bool clientAvailable_=false;

};
}

QWidget* createDeviceManagerView(QWidget* owner)
{
    return new DeviceManagerView(owner);
}
}
