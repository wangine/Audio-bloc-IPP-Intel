#pragma once

#include <functional>

#include <QHash>
#include <QString>
#include <QStringList>

class QWidget;

namespace entropy::host
{
struct ModuleViewContext
{
    QString moduleId;
    QString instanceId;
    QString displayName;
    QString routeSummary;
    QString version;
    QString lifecycleState;
    QStringList semanticInputs;
    QStringList semanticOutputs;
    bool installed = false;
    bool controlAvailable = false;
    bool telemetryAvailable = false;
    int latencySamples = -1;
};

class ModuleViewRegistry final
{
public:
    using Factory = std::function<QWidget*(const ModuleViewContext&, QWidget*)>;

    ModuleViewRegistry();
    void registerView(const QString& moduleId, Factory factory);
    QWidget* create(const ModuleViewContext& context, QWidget* owner) const;
    bool hasDedicatedView(const QString& moduleId) const;

private:
    QHash<QString, Factory> factories_;
};
}
