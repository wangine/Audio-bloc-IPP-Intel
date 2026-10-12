#pragma once
#include <regex>
inline void stage2Interop(const std::wstring &sourcePipe, const std::filesystem::path &root,
                          const uint8_t *sourceSession) {
    for (unsigned attempt = 0; attempt < 2; attempt++) {
        auto stop = root / ("stage2-stop-" + std::to_string(attempt)),
             diag = root / ("stage2-diagnostic-" + std::to_string(attempt) + ".json");
        std::filesystem::remove(stop);
        std::wstring command = L"\"" TEST_STAGE2_EXE L"\" --headless --automatic --runtime-root \"" +
                               (root / "stage2-state").wstring() + L"\" --link-endpoint \"" + sourcePipe +
                               L"\" --stop-file \"" + stop.wstring() + L"\" --diagnostic-file \"" +
                               diag.wstring() + L"\"";
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION child{};
        CHECK(CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                             nullptr, &startup, &child));
        struct Cleanup {
            PROCESS_INFORMATION &p;
            ~Cleanup() {
                if (WaitForSingleObject(p.hProcess, 0) == WAIT_TIMEOUT) {
                    TerminateProcess(p.hProcess, 99);
                    WaitForSingleObject(p.hProcess, 2000);
                }
                CloseHandle(p.hProcess);
                CloseHandle(p.hThread);
            }
        } cleanup{child};
        entropy::publish::Subscriber output;
        std::string error;
        auto endpoint = sourcePipe + L".StagePublish.V2";
        bool connected = false;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
        while (std::chrono::steady_clock::now() < deadline &&
               WaitForSingleObject(child.hProcess, 0) == WAIT_TIMEOUT) {
            if (output.connect(endpoint, error)) {
                connected = true;
                break;
            }
            Sleep(20);
        }
        if (!connected)
            std::cerr << error << '\n';
        CHECK(connected);
        CHECK(output.session().layout_id == 4 && output.session().channels == 3);
        CHECK(!memcmp(output.session().session_id, sourceSession, 16));
        std::array<std::array<double, 1024>, 3> storage{};
        double *planes[] = {storage[0].data(), storage[1].data(), storage[2].data()};
        StageBusBlockHeader header{};
        unsigned blocks = 0;
        deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (blocks < 8 && std::chrono::steady_clock::now() < deadline) {
            LARGE_INTEGER q;
            QueryPerformanceCounter(&q);
            output.heartbeat(q.QuadPart);
            auto result = output.read(header, planes);
            if (result == entropy::publish::ReadResult::Block) {
                CHECK(!(header.validity_mask & ~3u) && header.source_generation[2] == 0);
                if (header.validity_mask & 1) {
                    blocks++;
                    CHECK(header.source_generation[0] == 1);
                }
            } else if (result == entropy::publish::ReadResult::Gap) {
                std::cout << "Stage2 test consumer GAP: bounded queue overflow; reconnecting without "
                             "changing producer\n";
                output.close();
                CHECK(output.connect(endpoint, error));
                CHECK(!memcmp(output.session().session_id, sourceSession, 16));
                blocks = 0;
            } else {
                if (result != entropy::publish::ReadResult::Empty)
                    std::cerr << "Stage2 read result " << int(result) << "\n";
                CHECK(result == entropy::publish::ReadResult::Empty);
            }
            Sleep(1);
        }
        CHECK(blocks == 8);
        output.close();
        std::ofstream(stop).put('1');
        CHECK(WaitForSingleObject(child.hProcess, 5000) == WAIT_OBJECT_0);
        DWORD code = 99;
        GetExitCodeProcess(child.hProcess, &code);
        CHECK(code == 0);
        std::ifstream report(diag);
        std::string json((std::istreambuf_iterator<char>(report)), {});
        CHECK(std::regex_search(json, std::regex("\"input_channels\"\\s*:\\s*2")));
        CHECK(std::regex_search(json, std::regex("\"output_channels\"\\s*:\\s*3")));
        CHECK(std::regex_search(json, std::regex("\"center_loaded\"\\s*:\\s*true")));
        CHECK(std::regex_search(json, std::regex("\"lrc_loaded\"\\s*:\\s*true")));
    }
    std::cout << "PASS MSVC Stage1 -> actual packaged MinGW Stage2 MUSIC2/LRC3 native DLLs; stop/restart "
                 "with stable source UUID\n";
}
