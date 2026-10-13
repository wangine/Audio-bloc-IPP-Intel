#include "DspPackage.h"
#include "TestSupport.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <iostream>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    CHECK(temp.isValid());
    const auto root = std::filesystem::path(temp.path().toStdWString());
    std::string error;
    CHECK(!verifyDspPackage(root, error));
    auto copy = [&](const std::filesystem::path& source, const std::string& relative) {
        auto target = root / relative;
        std::filesystem::create_directories(target.parent_path());
        std::filesystem::copy_file(source, target);
    };
    auto hashOf = [&](const std::string& relative) {
        QFile file(QString::fromStdWString((root / relative).wstring()));
        if (!file.open(QIODevice::ReadOnly)) return QString();
        return QString::fromLatin1(QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex());
    };
    auto entry = [&](const std::string& relative, bool optional = false) {
        return QJsonObject{{"path", QString::fromStdString(relative)}, {"sha256", hashOf(relative)}, {"optional", optional}};
    };
    QFile exe(QString::fromStdWString((root / "EntropyDspEngine.exe").wstring()));
    CHECK(exe.open(QIODevice::WriteOnly));
    CHECK(exe.write("payload") == 7);
    exe.close();
    const auto sdk = std::filesystem::path(TEST_SDK_ROOT);
    copy(sdk / "StageBusContract.json", "sdk/StageBusContract.json");
    copy(sdk.parent_path().parent_path().parent_path() / "contract/MusicStageBusContract.json", "sdk/MusicStageBusContract.json");
    copy(root / "EntropyDspEngine.exe", "sdk/include/entropy/stage_bus_v1.h");
    QJsonArray files{entry("EntropyDspEngine.exe"), entry("sdk/StageBusContract.json"),
        entry("sdk/MusicStageBusContract.json"), entry("sdk/include/entropy/stage_bus_v1.h")};
    QJsonObject manifest{{"stage", "DSP_STAGE2"},
        {"stagebus_contract_sha256", hashOf("sdk/StageBusContract.json")},
        {"music_stagebus_contract_sha256", hashOf("sdk/MusicStageBusContract.json")},
        {"input_channels", 2}, {"output_channels", 3}, {"files", files}};
    auto write = [&] {
        QFile file(QString::fromStdWString((root / "package-manifest.json").wstring()));
        return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(manifest).toJson()) > 0;
    };
    CHECK(write());
    CHECK(verifyDspPackage(root, error));

    QJsonObject recovery{{"path", "modules/EntropyDynamicRecovery.dll"},
        {"sha256", "84fbb4f7e2519eaaa13c68159fb596d44ffda2f0de9b8bde2b2d72de7573973d"},
        {"optional", true}};
    files.append(recovery);
    manifest.insert("dynamic_recovery_available", true);
    manifest.insert("files", files);
    CHECK(write());
    CHECK(verifyDspPackage(root, error));
    auto bad = recovery;
    bad.insert("sha256", QString(64, '0'));
    files[files.size() - 1] = bad;
    manifest.insert("files", files);
    CHECK(write());
    CHECK(!verifyDspPackage(root, error));
    files[files.size() - 1] = recovery;
    manifest.insert("files", files);
    copy(std::filesystem::path(CENTER_MODULE_ROOT) / "EntropyDynamicRecovery.dll", "modules/EntropyDynamicRecovery.dll");
    CHECK(write());
    CHECK(verifyDspPackage(root, error));
    QFile dynamic(QString::fromStdWString((root / "modules/EntropyDynamicRecovery.dll").wstring()));
    CHECK(dynamic.open(QIODevice::Append));
    CHECK(dynamic.write("tamper") == 6);
    dynamic.close();
    CHECK(!verifyDspPackage(root, error));
    std::filesystem::remove(root / "modules/EntropyDynamicRecovery.dll");
    CHECK(verifyDspPackage(root, error));
    QFile extra(QString::fromStdWString((root / "modules/EntropyDacSink.dll").wstring()));
    CHECK(extra.open(QIODevice::WriteOnly));
    extra.write("extra");
    extra.close();
    CHECK(!verifyDspPackage(root, error));
    std::cout << "PASS pinned optional recovery, missing module, tamper and unknown DLL\n";
}
