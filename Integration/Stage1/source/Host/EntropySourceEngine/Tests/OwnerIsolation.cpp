// Test executables only: keep the real Windows mutex semantics in a private namespace.
#include <windows.h>
#include <string>
#include <cwchar>
namespace {
HANDLE WINAPI testCreateMutex(LPSECURITY_ATTRIBUTES a,BOOL owner,LPCWSTR name)
{
    using Function=HANDLE(WINAPI*)(LPSECURITY_ATTRIBUTES,BOOL,LPCWSTR);
    static auto real=reinterpret_cast<Function>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"CreateMutexW"));
    std::wstring isolated;
    if(name && !std::wcscmp(name,L"Local\\Entropy.Stage1.Source.Owner.V1")) {
        wchar_t suffix[128]{};
        GetEnvironmentVariableW(L"ENTROPY_SOURCE_TEST_NAMESPACE",suffix,128);
        isolated=L"Local\\Entropy.Stage1.Tests.Owner."+std::wstring(suffix[0]?suffix:std::to_wstring(GetCurrentProcessId()).c_str());
        name=isolated.c_str();
    }
    return real(a,owner,name);
}
}
extern "C" { decltype(&testCreateMutex) __wrap___imp_CreateMutexW = &testCreateMutex; }
