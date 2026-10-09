#include "SourceState.h"
#include "ProjectPersistence.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSettings>
#include <cmath>
namespace entropy::host
{
namespace
{
QString path(const std::filesystem::path &p) { return QString::fromStdWString(p.wstring()); }
bool readBounded(const std::filesystem::path &p, QByteArray &data, std::string &error)
{
    QFile file(path(p));
    if (!file.open(QIODevice::ReadOnly) || file.size() > 32 * 1024 * 1024)
    {
        error = "Import missing/unreadable or exceeds 32 MiB";
        return false;
    }
    data = file.readAll();
    if (file.error() != QFile::NoError)
    {
        error = "Incomplete import read";
        return false;
    }
    return true;
}
bool number(const QJsonValue &v, uint32_t &out)
{
    auto n = v.toDouble(-1);
    if (!v.isDouble() || n < 0 || n > 4294967295.0 || std::floor(n) != n)
        return false;
    out = static_cast<uint32_t>(n);
    return true;
}
} // namespace
bool SourceState::importR10(const std::filesystem::path &project, const std::filesystem::path &ini,
                            std::string &error)
{
    error.clear();
    EntropyProjectDocument validated;
    auto result = loadProject(project, validated);
    if (!result.ok || validated.hostEngineRate != 192000)
    {
        error = "R10 import invalid: " + result.detail;
        return false;
    }
    QByteArray p, i;
    if (!readBounded(project, p, error) || !readBounded(ini, i, error))
        return false;
    QSettings iniCheck(path(ini), QSettings::IniFormat);
    iniCheck.setFallbacksEnabled(false);
    iniCheck.allKeys();
    if (iniCheck.status() != QSettings::NoError)
    {
        error = "R10 INI invalid or unreadable";
        return false;
    }
    QJsonObject candidate = opaque_;
    candidate["r10_project_base64"] = QString::fromLatin1(p.toBase64());
    candidate["r10_ini_base64"] = QString::fromLatin1(i.toBase64());
    candidate["r10_project_sha256"] =
        QString::fromLatin1(QCryptographicHash::hash(p, QCryptographicHash::Sha256).toHex());
    candidate["r10_ini_sha256"] =
        QString::fromLatin1(QCryptographicHash::hash(i, QCryptographicHash::Sha256).toHex());
    candidate["source_ini_keys_present"] = QJsonArray{};
    opaque_ = candidate;
    return true;
}
bool SourceState::save(const SourceStateSnapshot &state, std::string &error)
{
    error.clear();
    if (state.manualMuteMask > 7 || !state.revision || state.leftChannel == state.rightChannel ||
        !SourceStartupPolicy::permitted(state.current))
    {
        error = "Invalid Source snapshot";
        return false;
    }
    auto document = opaque_;
    document["schema"] = 1;
    document["startup_music"] = "MUSIC_TRANSPORT";
    document["manual_mute_mask"] = static_cast<int>(state.manualMuteMask);
    document["revision"] = QString::number(state.revision);
    QJsonArray engines;
    for (auto v : state.engines)
        engines.append(static_cast<double>(v));
    document["src_engines"] = engines;
    document["music_left"] = static_cast<double>(state.leftChannel);
    document["music_right"] = static_cast<double>(state.rightChannel);
    document["current_selection"] =
        QJsonObject{{"provider", QString::fromStdString(state.current.providerId)},
                    {"endpoint", QString::fromStdString(state.current.endpointId)},
                    {"access", static_cast<int>(state.current.accessMode)}};
    if (!QDir().mkpath(path(root_)))
    {
        error = "Cannot create private state root";
        return false;
    }
    QSaveFile file(path(root_ / "SourceState.json"));
    file.setDirectWriteFallback(false);
    auto data = QJsonDocument(document).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
    {
        error = "Atomic Source save failed";
        return false;
    }
    opaque_ = document;
    return true;
}
bool SourceState::load(SourceStateSnapshot &state, std::string &error)
{
    error.clear();
    QByteArray data;
    if (!readBounded(root_ / "SourceState.json", data, error))
        return false;
    QJsonParseError parse;
    auto json = QJsonDocument::fromJson(data, &parse);
    auto o = json.object();
    SourceStateSnapshot candidate;
    bool revisionValid = false;
    candidate.revision = o.value("revision").toString().toULongLong(&revisionValid);
    auto engines = o.value("src_engines").toArray();
    auto key = o.value("current_selection").toObject();
    candidate.current.providerId = key.value("provider").toString().toStdString();
    candidate.current.endpointId = key.value("endpoint").toString().toStdString();
    if (parse.error != QJsonParseError::NoError || !json.isObject() ||
        o.value("schema").toInt() != 1 ||
        o.value("startup_music").toString() != "MUSIC_TRANSPORT" || !revisionValid ||
        !candidate.revision || !number(o.value("manual_mute_mask"), candidate.manualMuteMask) ||
        candidate.manualMuteMask > 7 || engines.size() != 3 ||
        !number(o.value("music_left"), candidate.leftChannel) ||
        !number(o.value("music_right"), candidate.rightChannel) ||
        candidate.leftChannel == candidate.rightChannel ||
        !number(key.value("access"), candidate.current.accessMode) ||
        !SourceStartupPolicy::permitted(candidate.current))
    {
        error = "Source profile invalid/incompatible";
        return false;
    }
    for (unsigned k = 0; k < 3; ++k)
        if (!number(engines[k], candidate.engines[k]))
        {
            error = "Invalid SRC engine identity";
            return false;
        }
    opaque_ = o;
    state = candidate;
    return true;
}
} // namespace entropy::host
