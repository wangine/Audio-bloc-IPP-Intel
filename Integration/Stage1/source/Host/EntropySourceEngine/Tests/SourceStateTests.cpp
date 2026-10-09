#include "SourceState.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <iostream>
#define CHECK(x)                                                                                   \
    do                                                                                             \
    {                                                                                              \
        if (!(x))                                                                                  \
        {                                                                                          \
            std::cerr << "FAIL " << __LINE__ << ": " << #x << " " << error << "\n";                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
using namespace entropy::host;
QByteArray read(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir tmp;
    std::string error;
    SourceState state(tmp.path().toStdWString());
    auto project = std::filesystem::path(TEST_BASELINE_STATE) / "EntropyProject.entropy";
    auto ini = std::filesystem::path(TEST_BASELINE_STATE) / "Entropy Audio" / "Entropy Audio.ini";
    auto original = read(QString::fromStdWString(project.wstring()));
    auto iniOriginal = read(QString::fromStdWString(ini.wstring()));
    CHECK(!original.isEmpty());
    CHECK(state.importR10(project, ini, error));
    CHECK(read(QString::fromStdWString(project.wstring())) == original &&
          read(QString::fromStdWString(ini.wstring())) == iniOriginal);
    CHECK(QByteArray::fromBase64(
              state.preserved().value("r10_project_base64").toString().toLatin1()) == original);
    CHECK(QByteArray::fromBase64(state.preserved().value("r10_ini_base64").toString().toLatin1()) ==
          iniOriginal);
    SourceStateSnapshot input;
    input.manualMuteMask = 5;
    input.engines = {1, 0, 2};
    input.revision = 9007199254740993ull;
    CHECK(state.save(input, error));
    SourceStateSnapshot output;
    CHECK(state.load(output, error));
    CHECK(state.load(output, error));
    CHECK(output.revision == input.revision && output.manualMuteMask == 5 &&
          output.engines == input.engines);
    CHECK(SourceStartupPolicy::musicKey().providerId == "audio.entropy.endpoint.music-transport");
    QFile invalid(tmp.path() + "/bad.entropy");
    CHECK(invalid.open(QIODevice::WriteOnly));
    invalid.write("garbage");
    invalid.close();
    CHECK(!state.importR10(std::filesystem::path(tmp.path().toStdWString()) / "bad.entropy", ini,
                           error));
    CHECK(state.load(output, error) && output.revision == input.revision);
    std::cout << "PASS read-only R10 import, opaque bytes, atomic private Save/Load, local lifecycle, "
                 "uint64 precision, invalid import preserves profile\n";
}
