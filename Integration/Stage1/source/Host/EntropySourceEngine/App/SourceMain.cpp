#include "SourceController.h"
#include "SourceUiBridge.h"
#include "SourceWindow.h"
#include "SourceLinkPackage.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFontDatabase>
#include <QTimer>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
using namespace entropy::host;
namespace {
void writeSourceStatus(const SourceController& controller,const QString& target)
{
    if(target.isEmpty())return;
    const auto s=controller.snapshot();
    const QJsonObject status{{"external",s.external},{"capturing",s.capturing},
        {"faults",static_cast<qint64>(s.faults)},{"blocks",static_cast<qint64>(s.blocks)},
        {"sequence",static_cast<qint64>(s.sequence)},{"firstFrame",static_cast<qint64>(s.firstFrame)},
        {"publishDrops",static_cast<qint64>(s.publishDrops)},
        {"session",QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(s.session.session_id),16).toHex())}};
    QSaveFile file(target);file.setDirectWriteFallback(false);
    if(file.open(QIODevice::WriteOnly)){file.write(QJsonDocument(status).toJson());file.commit();}
}
}
int main(int argc, char **argv)
{
    bool verify = false, smoke = false, offline = false, initialize = false, headless=false;
    for (int i = 1; i < argc; ++i)
    {
        verify |= std::string(argv[i]) == "--verify-modules";
        verify |= std::string(argv[i]) == "--verify-only";
        headless |= std::string(argv[i]) == "--headless";
        smoke |= std::string(argv[i]) == "--smoke-test";
        offline |= std::string(argv[i]) == "--offline";
        initialize |= std::string(argv[i]) == "--initialize-profile";
    }
    bool testSmoke=false;
#ifdef ENTROPY_SOURCE_TEST_ENGINE
    testSmoke=smoke;
#endif
    wchar_t executable[32768]{};auto length=GetModuleFileNameW(nullptr,executable,32768);std::string packageError;
    if(!length || length>=32768 || (!testSmoke && !entropy::stage::verifySourceLinkPackage(std::filesystem::path(executable).parent_path(),packageError))){std::fprintf(stderr,"%s\n",packageError.c_str());return 2;}
    if(headless){
        QCoreApplication app(argc,argv);auto args=app.arguments();auto value=[&](const char* name){auto i=args.indexOf(name);return i>=0 && i+1<args.size()?args[i+1]:QString();};auto root=value("--runtime-root");if(root.isEmpty())root=QCoreApplication::applicationDirPath();auto endpoint=value("--link-endpoint");auto stop=value("--stop-file"),status=value("--status-file");if(stop.isEmpty())return 2;
        SourceController controller;std::string error;auto path=std::filesystem::path(root.toStdWString());
        auto publication=value("--publication-endpoint");
        if(!controller.enablePublication(publication.isEmpty()?entropy::publish::sourceEndpointName():publication.toStdWString(),error) || !controller.enableStageLink(endpoint.isEmpty()?entropy::link::endpointName():endpoint.toStdWString(),error) || !controller.start(path/"modules",path/"state",error,offline)){std::fprintf(stderr,"%s\n",error.c_str());return 3;}
        while(!std::filesystem::exists(std::filesystem::path(stop.toStdWString()))){
            writeSourceStatus(controller,status);
            Sleep(20);
        }controller.stop();return 0;
    }
    if (initialize)
    {
        QCoreApplication app(argc, argv);
        auto root = std::filesystem::path(QCoreApplication::applicationDirPath().toStdWString());
        QString project, ini;
        for (int i = 1; i + 1 < argc; ++i)
        {
            if (std::string(argv[i]) == "--r10-project")
                project = QString::fromLocal8Bit(argv[++i]);
            else if (std::string(argv[i]) == "--r10-ini")
                ini = QString::fromLocal8Bit(argv[++i]);
        }
        if (std::filesystem::exists(root / "state" / "SourceState.json"))
        {
            std::fprintf(stderr, "Profile already exists; refusing overwrite\\n");
            return 5;
        }
        SourceState state(root / "state");
        std::string error;
        SourceStateSnapshot defaults;
        if (project.isEmpty() || ini.isEmpty() ||
            !state.importR10(project.toStdWString(), ini.toStdWString(), error) ||
            !state.save(defaults, error))
        {
            std::fprintf(stderr, "%s\\n", error.c_str());
            return 6;
        }
        std::printf("PASS initialized private Source profile with opaque read-only R10 import\\n");
        return 0;
    }
    if (verify)
    {
        QCoreApplication app(argc, argv);
        auto root = std::filesystem::path(QCoreApplication::applicationDirPath().toStdWString());
        std::string error;
        if (!SourceController::verifyModules(root / "modules", error))
        {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 2;
        }
        std::printf("PASS Stage1 six validated Source modules; no capture\n");
        return 0;
    }
    QApplication app(argc, argv);
    if (smoke && QApplication::platformName() == "offscreen")
    {
        const auto fontId = QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") +
                                                              "/Fonts/segoeui.ttf");
        const auto families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty())
            app.setFont(QFont(families.front(), 9));
    }
    app.setApplicationName("Entropy Source Engine");
    app.setOrganizationName("Entropy Stage1");
    auto root = std::filesystem::path(QCoreApplication::applicationDirPath().toStdWString());
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--runtime-root")
            root = std::filesystem::absolute(QString::fromLocal8Bit(argv[++i]).toStdWString());
    SourceController controller;
    std::string error;
    auto uiEndpoint=entropy::link::endpointName();
    auto publicationEndpoint=entropy::publish::sourceEndpointName();
    QString statusFile;
    for(int i=1;i+1<argc;++i) {
        if(std::string(argv[i])=="--publication-endpoint")publicationEndpoint=QString::fromLocal8Bit(argv[++i]).toStdWString();
        else if(std::string(argv[i])=="--status-file")statusFile=QString::fromLocal8Bit(argv[++i]);
    }
#ifdef ENTROPY_SOURCE_TEST_ENGINE
    if (testSmoke) { uiEndpoint += L".smoke." + std::to_wstring(GetCurrentProcessId()); publicationEndpoint += L".smoke." + std::to_wstring(GetCurrentProcessId()); }
#endif
    if (!controller.enablePublication(publicationEndpoint,error)) {std::fprintf(stderr,"%s\n",error.c_str());return 3;}
    if (!controller.enableStageLink(uiEndpoint,error)) {std::fprintf(stderr,"%s\n",error.c_str());return 3;}
    if (!controller.start(root / "modules", root / "state", error, offline || smoke))
    {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 3;
    }
    SourceWindow window(controller);
    window.show();
    QTimer statusTimer;
    if(!statusFile.isEmpty()) {
        QObject::connect(&statusTimer,&QTimer::timeout,&app,[&]{writeSourceStatus(controller,statusFile);});
        statusTimer.start(20);
    }
    if (smoke)
        QTimer::singleShot(150, &window,
                           [&]
                           {
                               window.grab().save(QString::fromStdWString(
                                   (root / "state" / "ui-smoke.png").wstring()));
                           });
    if (smoke)
    {
        QTimer::singleShot(200, &window, [&window] { window.showMinimized(); });
        QTimer::singleShot(
            1400, &app,
            [&]
            {
                auto s = controller.snapshot();
                std::printf(
                    "Source smoke blocks=%llu minimized=%d captures=%d faults=%llu\n",
                    static_cast<unsigned long long>(s.blocks), window.isMinimized(), s.capturing,
                    static_cast<unsigned long long>(s.faults));
                app.exit(s.blocks > 30 && window.isMinimized() && !s.capturing &&
                                 !s.faults
                             ? 0
                             : 4);
            });
    }
    int result = app.exec();
    attachSourceUi(nullptr);
    controller.stop();
    return result;
}
