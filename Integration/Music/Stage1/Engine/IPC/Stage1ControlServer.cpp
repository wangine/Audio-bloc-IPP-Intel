#include "Stage1ControlServer.h"
#include "StageLinkSecurity.h"
#include <objbase.h>
namespace entropy::music {
Stage1ControlServer::~Stage1ControlServer() {
    stop();
    CloseHandle(stop_);
}
void Stage1ControlServer::stop() {
    SetEvent(stop_);
    if (worker_.joinable())
        worker_.join();
}
bool Stage1ControlServer::start(const std::wstring &name, EngineLifecycle &engine) {
    if (!stop_ || worker_.joinable())
        return false;
    worker_ = std::thread([this, name, &engine] {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        entropy::link::PipeSecurity security;
        while (security.valid() && WaitForSingleObject(stop_, 0) != WAIT_OBJECT_0) {
            HANDLE pipe =
                CreateNamedPipeW(name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                 PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                                 1, 65536, 65536, 0, security.attributes());
            if (pipe == INVALID_HANDLE_VALUE)
                break;
            OVERLAPPED connect{};
            connect.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            bool connected = ConnectNamedPipe(pipe, &connect) != FALSE;
            auto code = GetLastError();
            if (!connected && code == ERROR_PIPE_CONNECTED)
                connected = true;
            else if (!connected && code == ERROR_IO_PENDING) {
                HANDLE waits[] = {stop_, connect.hEvent};
                if (WaitForMultipleObjects(2, waits, FALSE, INFINITE) == WAIT_OBJECT_0 + 1) {
                    DWORD n;
                    connected = GetOverlappedResult(pipe, &connect, &n, FALSE) != FALSE;
                } else {
                    CancelIoEx(pipe, &connect);
                    DWORD n;
                    GetOverlappedResult(pipe, &connect, &n, TRUE);
                }
            }
            if (connected) {
                Stage1ControlRequest request{};
                if (entropy::link::transfer(pipe, false, &request, sizeof(request), 2000, stop_)) {
                    auto response = engine.control(request);
                    entropy::link::transfer(pipe, true, &response, sizeof(response), 2000, stop_);
                }
            }
            CloseHandle(connect.hEvent);
            DisconnectNamedPipe(pipe);
            CloseHandle(pipe);
        }
        CoUninitialize();
    });
    return true;
}
} // namespace entropy::music
