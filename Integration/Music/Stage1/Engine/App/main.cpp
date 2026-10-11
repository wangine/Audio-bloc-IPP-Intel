#include "EngineLifecycle.h"
#include "Stage1ControlServer.h"
#include "StageLinkSecurity.h"
#include "StagePublishV2.h"
#include "Telemetry.h"
#include "WindowsEndpointCatalog.h"
#include <iostream>
#include <map>
#include <thread>
int wmain(int argc, wchar_t **argv) {
    using namespace entropy::music;
    try {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        std::map<std::wstring, std::wstring> args;
        for (int i = 1; i < argc; i += 2) {
            if (i + 1 == argc)
                throw std::runtime_error("Missing CLI value");
            args[argv[i]] = argv[i + 1];
        }
        auto directory = std::filesystem::absolute(argv[0]).parent_path();
        auto value = [&](const wchar_t *key, const std::wstring &fallback) {
            auto it = args.find(key);
            return it == args.end() ? fallback : it->second;
        };
        auto config = value(L"--config", (directory / L"state" / L"Stage1Music.ini").wstring());
        auto providers = value(L"--provider-directory", (directory / L"providers").wstring());
        auto publish = value(L"--publish-endpoint", entropy::publish::sourceEndpointName());
        auto control = value(L"--control-endpoint", publish + L".Control.V1");
        auto stopFile = value(L"--stop-file", L"");
        auto diagnostic = value(L"--diagnostic-file", L"");
        entropy::link::PipeSecurity security;
        auto mutexName =
            L"Local\\Entropy.Stage1.Music.Owner." + std::to_wstring(std::hash<std::wstring>{}(control));
        HANDLE owner = CreateMutexW(security.attributes(), TRUE, mutexName.c_str());
        if (!owner || GetLastError() == ERROR_ALREADY_EXISTS)
            throw std::runtime_error("Stage1 MUSIC owner already active");
        EngineLifecycle engine;
        std::string error;
        if (!engine.start(config, providers, publish, error))
            throw std::runtime_error(error);
        Stage1ControlServer server;
        if (!server.start(control, engine))
            throw std::runtime_error("Control server start failed");
        std::cout << "Stage1 MUSIC Core active; Qt independent; no DAC output\n";
        while (stopFile.empty() || !std::filesystem::exists(stopFile))
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        server.stop();
        auto final = engine.snapshot();
        engine.stop();
        if (!diagnostic.empty())
            writeDiagnostic(diagnostic, final);
        ReleaseMutex(owner);
        CloseHandle(owner);
        CoUninitialize();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
