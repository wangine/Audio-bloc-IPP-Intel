#include <windows.h>
#include <cstdio>
#include <string>
int wmain(int argc,wchar_t**argv){
 if(argc!=2)return 2;
 std::wstring root=argv[1];SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_USER_DIRS|LOAD_LIBRARY_SEARCH_SYSTEM32);
 auto cookie=AddDllDirectory(root.c_str());if(!cookie)return 3;
 const wchar_t* files[]={L"Qt6Widgets.dll",L"Qt6Network.dll",L"Qt6Svg.dll",L"modules/EntropyMusicTransportEndpoint.dll",L"modules/EntropyTidalEndpoint.dll",L"modules/EntropyAuxEndpoint.dll",L"modules/EntropyCinemaInput.dll",L"modules/EntropySampleRateConverter.dll",L"modules/EntropyInputMatrix.dll",L"platforms/qwindows.dll"};
 for(auto file:files){auto path=root+L"/"+file;if(!LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)){std::fwprintf(stderr,L"FAIL loading %ls: %lu\n",file,GetLastError());return 4;}}
 const wchar_t* dependencies[]={L"msvcp140.dll",L"vcruntime140.dll",L"vcruntime140_1.dll",L"Qt6Core.dll",L"Qt6Gui.dll",L"Qt6Widgets.dll",L"libstdc++-6.dll",L"libgcc_s_seh-1.dll",L"libwinpthread-1.dll"};
 wchar_t expected[32768],actual[32768],loaded[32768];
 for(auto name:dependencies){auto handle=GetModuleHandleW(name);if(!handle||!GetModuleFileNameW(handle,loaded,32768))return 5;auto path=root+L"/"+name;GetFullPathNameW(path.c_str(),32768,expected,nullptr);GetFullPathNameW(loaded,32768,actual,nullptr);if(_wcsicmp(expected,actual)){std::fwprintf(stderr,L"FAIL nonlocal dependency %ls at %ls\n",name,actual);return 6;}std::wprintf(L"PASS app-local %ls\n",name);}
 return 0;
}
