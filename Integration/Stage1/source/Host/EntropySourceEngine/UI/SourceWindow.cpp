#include "SourceWindow.h"
#include "LiveUiRefresh.h"
#include "ModuleViewRegistry.h"
#include "SourceStyle.h"
#include "SourceUiBridge.h"
#include "SrcPage.h"
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QScreen>
#include <QGuiApplication>
#include <cmath>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
namespace entropy::host
{
SourceWindow::SourceWindow(SourceController &controller)
{
    attachSourceUi(&controller);
    setWindowTitle("Entropy — Source Engine · Stage 1");
    resize(1360, 960);
    if (auto* screen = QGuiApplication::primaryScreen(); screen && QGuiApplication::platformName() != "offscreen")
        resize((std::min)(width(), screen->availableGeometry().width()-40),
               (std::min)(height(), screen->availableGeometry().height()-60));
    applySourceStyle(*this);
    auto *widget = new QWidget(this);
    setCentralWidget(widget);
    auto *root = new QVBoxLayout(widget);
    root->setContentsMargins(16, 12, 16, 10);
    root->setSpacing(8);
    auto* header = new QHBoxLayout;
    auto* branding = new QVBoxLayout;
    auto *brand = new QLabel("ENTROPY", widget);
    brand->setObjectName("brand");
    branding->addWidget(brand);
    auto *subtitle = new QLabel("MODULAR AUDIO PROCESSOR", widget);
    subtitle->setObjectName("subtitle");
    branding->addWidget(subtitle);
    header->addLayout(branding,1);
    auto* stageBadge = new QLabel("SOURCE ENGINE  •  STAGE 1", widget);
    stageBadge->setStyleSheet("color:#59d6ff;border:1px solid #27637a;padding:10px 16px;background:#0a3148;");
    header->addWidget(stageBadge);
    root->addLayout(header);
    auto* scroll = new QScrollArea(widget);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* page = new QWidget(scroll);
    auto* flow = new QHBoxLayout(page);
    flow->setContentsMargins(0,0,0,0);
    flow->setSpacing(12);
    auto* inputs = new QGroupBox("INPUT DOMAINS",page);
    auto* inputLayout = new QVBoxLayout(inputs);
    inputLayout->setSpacing(8);
    auto* srcGroup = new QGroupBox("SOURCE RATE CONVERSION",page);
    auto* srcLayout = new QVBoxLayout(srcGroup);
    srcLayout->setSpacing(8);
    std::array<QLabel*,3> inputStates{},inputFormats{},inputPeaks{};
    std::array<QFrame*,3> inputCards{};
    const char* roleNames[] = {"MUSIC","AUX","CINEMA"};
    const char* roleColors[] = {"#18bfff","#43df76","#a971ff"};
    for(unsigned role=0;role<3;++role) {
        auto* card = new QFrame(inputs);
        inputCards[role]=card;
        card->setObjectName(QString("inputCard%1").arg(role));
        card->setStyleSheet(QString("QFrame#inputCard%1 {background:#0a1d29;border:1px solid #24485d;border-left:4px solid %2;border-radius:6px;} ").arg(role).arg(roleColors[role]));
        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(12,8,12,8);
        layout->setSpacing(6);
        auto* title = new QLabel(roleNames[role],card);
        title->setObjectName("inputTitle");
        title->setAlignment(Qt::AlignCenter);
        layout->addWidget(title);
        inputFormats[role]=new QLabel(role==2 ? "8-channel endpoint\nNative format" : "Stereo endpoint\nNative format",card);
        inputFormats[role]->setAlignment(Qt::AlignCenter);
        inputFormats[role]->setWordWrap(true);
        layout->addWidget(inputFormats[role],1);
        inputPeaks[role]=new QLabel("— dBFS",card);
        inputPeaks[role]->setAlignment(Qt::AlignCenter);
        layout->addWidget(inputPeaks[role]);
        inputStates[role]=new QLabel("WAITING",card);
        inputStates[role]->setAlignment(Qt::AlignCenter);
        inputStates[role]->setObjectName("inputState");
        layout->addWidget(inputStates[role]);
        inputLayout->addWidget(card,role==0 ? 0 : 1);
        ModuleViewContext context;
        context.instanceId=role==0 ? "src.tidal" : role==1 ? "src.aux" : "src.cinema";
        context.moduleId="audio.entropy.src.multiengine.fp64";
        context.installed=context.controlAvailable=context.telemetryAvailable=true;
        auto* srcCard=createSrcView(context,srcGroup);
        srcCard->setObjectName(QString("srcCard%1").arg(role));
        srcCard->setStyleSheet(QString("QWidget#srcCard%1 {background:#0a1d29;border:1px solid #24485d;border-left:4px solid %2;border-radius:6px;}").arg(role).arg(roleColors[role]));
        srcLayout->addWidget(srcCard,1);
    }
    auto *transport = new QGroupBox("MUSIC SOURCE", inputCards[0]);
    auto *selection = new QGridLayout(transport);
    selection->setContentsMargins(6,6,6,6);
    selection->setSpacing(4);
    auto *source = new QComboBox(transport);
    source->setMinimumContentsLength(16);
    source->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    selection->addWidget(source,0,0,1,4);
    auto *bind = new QPushButton("SELECT SOURCE",transport);
    selection->addWidget(bind,1,0,1,2);
    auto *retry = new QPushButton("RETRY",transport);
    selection->addWidget(retry,1,2,1,2);
    qobject_cast<QVBoxLayout*>(inputCards[0]->layout())->insertWidget(2,transport);
    auto arrow = [&](const char* color) {
        auto* a=new QLabel("→",page);
        a->setAlignment(Qt::AlignCenter);
        a->setStyleSheet(QString("font-size:30px;color:%1;").arg(color));
        a->setFixedWidth(28);
        return a;
    };
    flow->addWidget(inputs,3);
    flow->addWidget(arrow("#18bfff"));
    flow->addWidget(srcGroup,4);
    flow->addWidget(arrow("#43df76"));
    auto* outputColumn=new QVBoxLayout;
    auto* matrixNode=new QGroupBox("INPUT MATRIX",page);
    auto* matrixLayout=new QVBoxLayout(matrixNode);
    auto* matrixCaption=new QLabel("12 CHANNELS  •  FP64\n\nProgramMixer\nMUSIC L/R + CINEMA 8ch",matrixNode);
    matrixCaption->setAlignment(Qt::AlignCenter);
    matrixCaption->setWordWrap(true);
    matrixLayout->addWidget(matrixCaption);
    outputColumn->addWidget(matrixNode);
    auto* down=new QLabel("↓",page);
    down->setAlignment(Qt::AlignCenter);
    down->setStyleSheet("font-size:25px;color:#43df76;");
    outputColumn->addWidget(down);
    auto *matrix = new QGroupBox("MATRIX OUT — 10 CHANNELS",page);
    auto *meters = new QGridLayout(matrix);
    std::array<QLabel *,10> readouts{};
    std::array<QProgressBar *,10> bars{};
    const char *names[10]={"MUSIC L","MUSIC R","CINEMA L","CINEMA R","CINEMA C","LFE","SL","SR","BL","BR"};
    for(unsigned c=0;c<10;++c) {
        auto* column=new QVBoxLayout;
        auto* label=new QLabel(names[c],matrix);
        label->setAlignment(Qt::AlignCenter);
        column->addWidget(label);
        bars[c]=new QProgressBar(matrix);
        bars[c]->setOrientation(Qt::Vertical);
        bars[c]->setRange(0,1000);bars[c]->setValue(0);
        bars[c]->setTextVisible(false);
        bars[c]->setFixedHeight(44);
        column->addWidget(bars[c],1,Qt::AlignHCenter);
        readouts[c]=new QLabel("WAITING",matrix);
        readouts[c]->setAlignment(Qt::AlignCenter);
        column->addWidget(readouts[c]);
        meters->addLayout(column,c/2,c%2);
    }
    outputColumn->addWidget(matrix,1);
    auto* bus=new QLabel("STAGEBUS V1\n10ch planar  •  FP64 / 192 kHz",page);
    bus->setAlignment(Qt::AlignCenter);
    bus->setStyleSheet("color:#59d6ff;border:1px solid #24485d;padding:10px;");
    outputColumn->addWidget(bus);
    flow->addLayout(outputColumn,3);
    scroll->setWidget(page);
    root->addWidget(scroll,1);
    auto *status = new QLabel("Starting Source…",widget);
    status->setWordWrap(true);
    root->addWidget(status);
    QObject::connect(bind, &QPushButton::clicked, this,
                     [&controller, source]
                     {
                         if (source->currentIndex() < 0)
                             return;
                         const auto key = source->currentData().toMap();
                         SourceCommand cmd;
                         cmd.type = SourceCommand::Select;
                         cmd.key = {key.value("provider").toString().toStdString(),
                                    key.value("endpoint").toString().toStdString(),
                                    key.value("access").toUInt()};
                         cmd.role = 0;
                         cmd.value = 1;
                         controller.command(cmd);
                     });
    QObject::connect(retry, &QPushButton::clicked, this,
                     [&controller]
                     {
                         SourceCommand cmd;
                         cmd.type = SourceCommand::Retry;
                         controller.command(cmd);
                     });
    auto *timer = new QTimer(this);
    timer->setInterval(100);
    QObject::connect(
        timer, &QTimer::timeout, this,
        [this, &controller, source, bind, status, bars, readouts, inputStates, inputFormats, inputPeaks]
        {
            if (!liveUiRefreshAllowed(this))
                return;
            auto snapshot = controller.snapshot();
            if (source->property("catalogRevision").toULongLong() != snapshot.catalog.revision)
            {
                auto selected = source->currentData();
                QSignalBlocker blocker(source);
                source->clear();
                for (const auto &item : snapshot.catalog.entries)
                {
                    if (!SourceStartupPolicy::permitted(item.key))
                        continue;
                    QVariantMap key{{"provider", QString::fromStdString(item.key.providerId)},
                                    {"endpoint", QString::fromStdString(item.key.endpointId)},
                                    {"access", item.key.accessMode}};
                    source->addItem(QString::fromStdString(item.name) +
                                        (item.selectable ? "" : " — UNAVAILABLE"),
                                    key);
                }
                auto index = source->findData(selected);
                if (index < 0)
                {
                    QVariantMap current{
                        {"provider", QString::fromStdString(snapshot.state.current.providerId)},
                        {"endpoint", QString::fromStdString(snapshot.state.current.endpointId)},
                        {"access", snapshot.state.current.accessMode}};
                    index = source->findData(current);
                    if (index < 0)
                    {
                        source->addItem("MUSIC identity — WAITING / UNAVAILABLE", current);
                        index = source->count() - 1;
                    }
                }
                if (index >= 0)
                    source->setCurrentIndex(index);
                source->setProperty("catalogRevision",
                                    QVariant::fromValue(snapshot.catalog.revision));
            }
            bind->setEnabled(snapshot.capturing);
            status->setText(
                QString::fromStdString(snapshot.reason) +
                (snapshot.sourceControlPending ? " · MUTE UPDATE PENDING SOURCE BLOCK" : ""));
            EngineTelemetrySnapshot telemetry{};
            controller.telemetry(telemetry);
            for(unsigned r=0;r<3;++r) {
                const auto& in=telemetry.inputs[r];
                const auto text=!in.bridgeConnected ? QString("DISCONNECTED")
                    : in.muted ? QString("CONNECTED / MUTED")
                    : in.endpointPeak>1.0e-8 ? QString("CONNECTED / SIGNAL ACTIVE")
                    : QString("CONNECTED / NO SIGNAL");
                const char* color=!in.bridgeConnected ? "#ff7070" : in.muted ? "#f0bd4c"
                    : in.endpointPeak>1.0e-8 ? "#43e878" : "#f0bd4c";
                if(inputStates[r]->text()!=text) {
                    inputStates[r]->setText(text);
                    inputStates[r]->setStyleSheet(QString("font-weight:700;color:%1;").arg(color));
                }
                inputFormats[r]->setText(QString("%1 Hz  •  %2 ch\n%3 valid / %4 container\nGeneration %5")
                    .arg(in.sourceRate).arg(in.channels).arg(in.validBits).arg(in.containerBits).arg(in.generation));
                inputPeaks[r]->setText(in.endpointPeak>1.0e-12
                    ? QString("%1 dBFS").arg(20*std::log10(in.endpointPeak),0,'f',1) : QString("−∞ dBFS"));
            }
            for (unsigned c = 0; c < 10; ++c)
            {
                unsigned role = c < 2 ? 0 : 2;
                auto &input = telemetry.inputs[role];
                double peak = snapshot.outputPeaks[c];
                bool valid = (snapshot.outputValidity & (c < 2 ? 3u : 4u)) != 0;
                bars[c]->setValue(static_cast<int>((std::min)(1.0, peak) * 1000));
                const bool requested = (snapshot.demandSourceMask & (1u << role)) != 0;
                readouts[c]->setText(!requested ? QString("STANDBY") : !valid ? QString("WAITING") : peak>1.0e-12
                    ? QString("%1 dBFS").arg(20*std::log10(peak),0,'f',1) : QString("−∞ dBFS"));
            }
            statusBar()->showMessage("Source delivery · all active inputs · FP64 / 192 kHz · " +
                                     QString(snapshot.capturing ? "RUNNING" : "STOPPED"));
        });
    timer->start();
}
} // namespace entropy::host
