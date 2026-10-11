#include "stage1_control_v1.h"
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <array>
#include <cmath>
#include <future>
#include <optional>
#include <windows.h>
#include <sddl.h>
#include <vector>
#include <windows.h>

namespace {
std::wstring defaultPipe() {
    HANDLE token{};
    std::wstring sid = L"unknown";
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        DWORD size = 0;
        GetTokenInformation(token, TokenUser, nullptr, 0, &size);
        std::vector<unsigned char> bytes(size);
        if (GetTokenInformation(token, TokenUser, bytes.data(), size, &size)) {
            LPWSTR value{};
            if (ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(bytes.data())->User.Sid, &value)) {
                sid = value;
                LocalFree(value);
            }
        }
        CloseHandle(token);
    }
    return L"\\\\.\\pipe\\Entropy.StagePublish.Music.Source.V2." + sid + L".Control.V1";
}
bool transfer(HANDLE pipe, bool write, void *data, DWORD size) {
    OVERLAPPED io{};
    io.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!io.hEvent)
        return false;
    DWORD offset = 0;
    bool ok = true;
    while (offset < size) {
        ResetEvent(io.hEvent);
        DWORD done = 0;
        auto ptr = static_cast<unsigned char *>(data) + offset;
        BOOL immediate = write ? WriteFile(pipe, ptr, size - offset, &done, &io)
                               : ReadFile(pipe, ptr, size - offset, &done, &io);
        if (!immediate) {
            if (GetLastError() != ERROR_IO_PENDING) {
                ok = false;
                break;
            }
            if (WaitForSingleObject(io.hEvent, 1000) != WAIT_OBJECT_0) {
                CancelIoEx(pipe, &io);
                GetOverlappedResult(pipe, &io, &done, TRUE);
                ok = false;
                break;
            }
            if (!GetOverlappedResult(pipe, &io, &done, FALSE)) {
                ok = false;
                break;
            }
        }
        if (!done) {
            ok = false;
            break;
        }
        offset += done;
    }
    CloseHandle(io.hEvent);
    return ok;
}
std::optional<Stage1ControlResponse> transact(const std::wstring &name, Stage1ControlRequest request) {
    request.magic = STAGE1_CONTROL_MAGIC;
    request.abiVersion = STAGE1_CONTROL_ABI;
    request.structSize = sizeof(request);
    WaitNamedPipeW(name.c_str(), 300);
    HANDLE pipe = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                              FILE_FLAG_OVERLAPPED, nullptr);
    if (pipe == INVALID_HANDLE_VALUE)
        return {};
    Stage1ControlResponse reply{};
    bool ok = transfer(pipe, true, &request, sizeof(request)) && transfer(pipe, false, &reply, sizeof(reply));
    CloseHandle(pipe);
    if (!ok || reply.magic != STAGE1_CONTROL_MAGIC || reply.abiVersion != STAGE1_CONTROL_ABI ||
        reply.structSize != sizeof(reply) || reply.endpointCount > STAGE1_CONTROL_ENDPOINTS)
        return {};
    return reply;
}
struct Reply : Stage1ControlResponse {
    std::vector<Stage1CatalogEntry> catalog;
    bool hasCatalog = false;
};
QString peakText(double peak) {
    return peak > 0 ? QString::number(20 * std::log10(peak), 'f', 2) : QString("−inf");
}
QString format(const Stage1SourceSnapshot &s) {
    if (!s.rate)
        return "UNKNOWN";
    QString rate = QString::number(s.rate / 1000., 'g', 5);
    return (s.validBits ? QString::number(s.validBits) : "UNKNOWN") + "/" + rate;
}
} // namespace
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    auto args = app.arguments();
    std::wstring pipe = defaultPipe();
    QString smoke;
    wchar_t windowsDirectory[MAX_PATH]{};
    if (GetWindowsDirectoryW(windowsDirectory, MAX_PATH)) {
        auto font = QString::fromWCharArray(windowsDirectory) + "/Fonts/segoeui.ttf";
        auto id = QFontDatabase::addApplicationFont(font);
        auto families = QFontDatabase::applicationFontFamilies(id);
        if (!families.isEmpty())
            app.setFont(QFont(families.first(), 10));
    }
    for (int i = 1; i + 1 < args.size(); i += 2) {
        if (args[i] == "--control-endpoint")
            pipe = args[i + 1].toStdWString();
        if (args[i] == "--smoke-file")
            smoke = args[i + 1];
    }
    QWidget window;
    window.setWindowTitle("Entropy · Stage 1 MUSIC");
    window.resize(980, 700);
    auto layout = new QVBoxLayout(&window);
    auto title = new QLabel("STAGE 1  ·  MUSIC SOURCE");
    title->setStyleSheet("font-size:24px;font-weight:600;padding:12px 0");
    layout->addWidget(title);
    auto clock = new QLabel("Clock: waiting for Core");
    layout->addWidget(clock);
    QComboBox *source[2];
    QComboBox *mode[2];
    QLabel *info[2];
    QPushButton *apply[2];
    for (unsigned i = 0; i < 2; i++) {
        auto group = new QGroupBox(i ? "AUX" : "MUSIC");
        auto grid = new QGridLayout(group);
        source[i] = new QComboBox;
        mode[i] = new QComboBox;
        apply[i] = new QPushButton("Select");
        info[i] = new QLabel("Input: UNKNOWN");
        info[i]->setWordWrap(true);
        info[i]->setMinimumHeight(100);
        source[i]->setMinimumWidth(480);
        source[i]->addItem("None");
        grid->addWidget(source[i], 0, 0);
        grid->addWidget(mode[i], 0, 1);
        grid->addWidget(apply[i], 0, 2);
        grid->addWidget(info[i], 1, 0, 1, 3);
        layout->addWidget(group);
    }
    auto output = new QLabel("OUTPUT   L/R · FP64 / 192 kHz\nUnity source level");
    output->setStyleSheet("font-size:16px;padding:10px 0");
    layout->addWidget(output);
    auto message = new QLabel("Connecting to native Core…");
    message->setWordWrap(true);
    layout->addWidget(message);
    auto refresh = new QPushButton("Refresh devices");
    layout->addWidget(refresh);
    layout->addStretch();
    window.setStyleSheet("QWidget{background:#18212b;color:#e6edf3;font-size:14px;} "
                         "QComboBox,QPushButton{background:#263544;padding:7px;border:1px solid "
                         "#465768;border-radius:4px;}QGroupBox{border:1px solid "
                         "#465768;border-radius:6px;margin-top:12px;padding:14px;}QGroupBox::title{"
                         "subcontrol-origin:margin;left:12px;}QPushButton:disabled{color:#758392;}");
    std::vector<Stage1CatalogEntry> catalog;
    std::future<std::optional<Reply>> pending;
    // STATUS is continuously in flight. Keep the latest user choice per role
    // until the current transaction completes instead of dropping the click.
    std::array<std::optional<Stage1ControlRequest>, 2> queuedSelection;
    bool enumerated = false, initial = true;
    auto send = [&](Stage1ControlRequest r) {
        if (pending.valid())
            return;
        pending = std::async(std::launch::async, [pipe, r]() mutable -> std::optional<Reply> {
            Reply result{};
            for (unsigned page = 0; page < 128; page++) {
                auto response = transact(pipe, r);
                if (!response)
                    return {};
                if (!page)
                    static_cast<Stage1ControlResponse &>(result) = *response;
                if (r.operation != STAGE1_OP_ENUMERATE)
                    return result;
                result.hasCatalog = true;
                result.catalog.insert(result.catalog.end(), response->endpoints,
                                      response->endpoints + response->endpointCount);
                if (!response->truncated) {
                    result.truncated = 0;
                    return result;
                }
                if (!response->endpointCount)
                    return {};
                r.catalogOffset += response->endpointCount;
            }
            return {};
        });
    };
    auto modes = [&](unsigned i) {
        mode[i]->clear();
        int index = source[i]->currentData().toInt();
        apply[i]->setEnabled(index >= -1);
        if (index < 0 || size_t(index) >= catalog.size())
            return;
        uint32_t mask = catalog[index].endpoint.accessModes;
        for (uint32_t m = 1; m < 32; m++)
            if (mask & (1u << m))
                mode[i]->addItem(m == 1   ? "Shared"
                                 : m == 2 ? "Exclusive"
                                 : m == 4 ? "Private tap"
                                 : m == 8 ? "Music transport"
                                          : QString("Access %1").arg(m),
                                 m);
    };
    for (unsigned i = 0; i < 2; i++) {
        QObject::connect(source[i], &QComboBox::currentIndexChanged, &window, [&, i] { modes(i); });
        QObject::connect(apply[i], &QPushButton::clicked, &window, [&, i] {
            Stage1ControlRequest r{};
            r.operation = STAGE1_OP_SELECT;
            r.role = 1u << i;
            auto index = source[i]->currentData().toInt();
            if (index >= 0 && size_t(index) < catalog.size()) {
                const auto &e = catalog[index];
                memcpy(r.providerId, e.providerId, sizeof(r.providerId));
                memcpy(r.endpointId, e.endpoint.endpointId, sizeof(r.endpointId));
                if (mode[i]->currentIndex() < 0)
                    return;
                r.mode = mode[i]->currentData().toUInt();
            }
            queuedSelection[i] = r;
        });
    }
    QObject::connect(refresh, &QPushButton::clicked, &window, [&] { enumerated = false; });
    QTimer poll;
    poll.setInterval(250);
    QObject::connect(&poll, &QTimer::timeout, &window, [&] {
        if (pending.valid()) {
            if (pending.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
                return;
            auto response = pending.get();
            if (!response) {
                message->setText("Core unavailable · source engine runs independently of this window");
                enumerated = false;
                return;
            }
            const auto &r = *response;
            clock->setText(QString("Clock: %1 · %2 Hz · QPC %3 Hz · late blocks %4")
                               .arg(r.clockRunning ? "RUNNING" : "STOPPED")
                               .arg(r.outputRate)
                               .arg(r.clockQpcFrequency)
                               .arg(r.clockLateBlocks));
            message->setText(r.status ? QString::fromUtf8(r.message)
                                      : QString("Core connected · published %1 blocks%2")
                                            .arg(r.blocksPublished)
                                            .arg(r.truncated ? " · device list truncated" : ""));
            if (!enumerated && r.hasCatalog) {
                catalog = r.catalog;
                for (unsigned i = 0; i < 2; i++) {
                    auto previous = source[i]->currentText();
                    source[i]->blockSignals(true);
                    source[i]->clear();
                    source[i]->addItem("None", -1);
                    for (size_t n = 0; n < catalog.size(); n++) {
                        const auto &e = catalog[n];
                        auto text =
                            QString::fromUtf8(e.endpoint.name) + " [" + QString::fromUtf8(e.providerId) + "]";
                        source[i]->addItem(text, int(n));
                        if (!e.endpoint.eligible || !(e.endpoint.roles & (1u << i)) || !e.providerId[0])
                            qobject_cast<QStandardItemModel *>(source[i]->model())
                                ->item(source[i]->count() - 1)
                                ->setEnabled(false);
                    }
                    int selected = -1;
                    if (initial) {
                        for (size_t n = 0; n < catalog.size(); n++)
                            if (!strcmp(catalog[n].providerId, r.sources[i].providerId) &&
                                !strcmp(catalog[n].endpoint.endpointId, r.sources[i].endpointId))
                                selected = int(n) + 1;
                    } else
                        selected = source[i]->findText(previous);
                    if (selected >= 0)
                        source[i]->setCurrentIndex(selected);
                    source[i]->blockSignals(false);
                    modes(i);
                    if (initial && selected < 0 && r.sources[i].providerId[0]) {
                        source[i]->addItem(QString("UNAVAILABLE: %1 / %2")
                                               .arg(r.sources[i].providerId, r.sources[i].endpointId),
                                           -2);
                        source[i]->setCurrentIndex(source[i]->count() - 1);
                        apply[i]->setEnabled(false);
                    }
                }
                initial = false;
                enumerated = true;
            }
            const char *states[] = {"none", "waiting", "active", "unavailable", "fault"};
            for (unsigned i = 0; i < 2; i++) {
                const auto &s = r.sources[i];
                info[i]->setText(QString("Input: %1 · %2 · %3\nSource clock: QPC %4 Hz · generation %5%6")
                                     .arg(format(s))
                                     .arg(s.state < 5 ? states[s.state] : "unknown")
                                     .arg(s.mode == 1   ? "Shared"
                                          : s.mode == 2 ? "Exclusive"
                                                        : "access UNKNOWN")
                                     .arg(s.qpcFrequency)
                                     .arg(s.generation)
                                     .arg(s.metadataFlags & ENTROPY_PACKET_METADATA_PARTIAL
                                              ? " · reference metadata"
                                              : ""));
                auto peaks =
                    s.state == 2
                        ? QString("\nSample Peak: %1 dBFS   ·   True Peak (4x est.): %2\nSRC Startup Peak: "
                                  "%3 dBFS   ·   %4/8192 frames%5")
                              .arg(peakText(std::max(s.prePeakL, s.prePeakR)))
                              .arg(s.truePeakReady ? peakText(std::max(s.truePeakL, s.truePeakR)) + " dBTP"
                                                   : "SETTLING")
                              .arg(peakText(std::max(s.startupPeakL, s.startupPeakR)))
                              .arg(s.startupFramesMeasured)
                              .arg(s.startupComplete ? " · latched" : " · collecting")
                        : QString(
                              "\nSample Peak: UNKNOWN   ·   True Peak: UNKNOWN\nSRC Startup Peak: UNKNOWN");
                info[i]->setText(info[i]->text() + peaks);
            }
            if (!smoke.isEmpty() && r.clockRunning) {
                window.grab().save(smoke);
                QJsonObject evidence;
                evidence["blocksPublished"] = qint64(r.blocksPublished);
                evidence["sessionId"] =
                    QString::fromLatin1(QByteArray(reinterpret_cast<const char *>(r.sessionId), 16).toHex());
                evidence["musicFormat"] = format(r.sources[0]);
                QFile file(smoke + ".json");
                if (file.open(QIODevice::WriteOnly))
                    file.write(QJsonDocument(evidence).toJson());
                app.exit(0);
                return;
            }
        }
        for (auto &selection : queuedSelection) {
            if (selection) {
                send(*selection);
                selection.reset();
                return;
            }
        }
        Stage1ControlRequest r{};
        r.operation = enumerated ? STAGE1_OP_STATUS : STAGE1_OP_ENUMERATE;
        send(r);
    });
    window.show();
    poll.start();
    if (!smoke.isEmpty())
        QTimer::singleShot(10000, &app, [&] { app.exit(3); });
    int result = app.exec();
    if (pending.valid())
        pending.wait();
    return result;
}
