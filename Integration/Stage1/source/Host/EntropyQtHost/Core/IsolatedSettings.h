#pragma once
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QString>

namespace entropy::host {
// Opt-in candidate profile. Native settings are read only during explicit seed.
inline bool configureIsolatedSettings(const QString& root, bool seed, QString& error) {
    const QFileInfo rootInfo(root);
    if (root.isEmpty() || !rootInfo.isAbsolute()) {
        error = "--settings-root requires an absolute directory"; return false;
    }
    const QString settingsFile = QDir(root).filePath("Entropy Audio/Entropy Audio.ini");
    if (seed) {
        if (QFileInfo::exists(settingsFile)) {
            error = "Profile already exists; refusing to overwrite it"; return false;
        }
        if (!QDir().mkpath(QFileInfo(settingsFile).absolutePath())) {
            error = "Cannot create profile directory"; return false;
        }
        QSettings original(QSettings::NativeFormat, QSettings::UserScope,
                           "Entropy Audio", "Entropy Audio");
        original.setFallbacksEnabled(false);
        const auto originalProject = original.value("Project/lastPath").toString();
        const auto copiedProject = QDir(root).filePath("EntropyProject.entropy");
        if (!originalProject.isEmpty() && !QFile::copy(originalProject, copiedProject)) {
            error = "Cannot copy the original project into the new profile"; return false;
        }
        QSettings isolated(settingsFile, QSettings::IniFormat);
        for (const auto& key : original.allKeys()) isolated.setValue(key, original.value(key));
        // An empty original lastPath is safe too: Save now targets the profile.
        isolated.setValue("Project/lastPath", copiedProject);
        isolated.sync();
        if (isolated.status() != QSettings::NoError) {
            error = "Cannot save isolated settings"; return false;
        }
    }
    if (!QFileInfo::exists(settingsFile)) {
        error = "Profile missing; initialize it explicitly first"; return false;
    }
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, root);
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, root);
    return true;
}
}
