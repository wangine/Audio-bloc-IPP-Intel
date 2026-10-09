#include "DspNativeRegistry.h"
#include "DspNativeHashes.h"
#include <bcrypt.h>
#include <array>
namespace entropy::stage {
const NativeModuleRecord* nativeModule(NativeModuleId id) noexcept {
 static constexpr NativeModuleRecord center{"EntropyCenterExtractorV2FP64.dll","audio.entropy.center-extractor.fp64-v2","e96bf561bfa204f7aada02e2245e264eb8db2e3f1e9a06ecc07caf43e0ba32f0"};
 static constexpr NativeModuleRecord lrc{"EntropyLrcAlignerFP64.dll","audio.entropy.lrc-aligner.fp64",lrcNativeSha};
 static constexpr NativeModuleRecord eq{"EntropyParametricEQ.dll","audio.entropy.parametric-eq.fp64","adb01f88cd58c3e4724422482c6970dfc7fb377d638dede2bf859ea87fd75e61"};
 switch(id){case NativeModuleId::CenterV2:return &center;case NativeModuleId::LrcAligner:return &lrc;case NativeModuleId::ParametricEq:return &eq;}return nullptr;
}
namespace {
bool verify(HANDLE file,const char* expectedSha) {
 BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
 if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
 bool ok=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0;
 std::array<unsigned char,65536> buffer{};DWORD count=0;
 while(ok){if(!ReadFile(file,buffer.data(),buffer.size(),&count,nullptr)){ok=false;break;}if(!count)break;ok=BCryptHashData(hash,buffer.data(),count,0)>=0;}
 std::array<unsigned char,32> digest{};
 if(ok)ok=BCryptFinishHash(hash,digest.data(),digest.size(),0)>=0;
 if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);
 constexpr char hex[]="0123456789abcdef";
 if(ok)for(unsigned i=0;i<32;i++)if(expectedSha[2*i]!=hex[digest[i]>>4] || expectedSha[2*i+1]!=hex[digest[i]&15])return false;
 return ok;
}
}
HMODULE loadNativeModule(NativeModuleId id,const std::filesystem::path& root,std::string& error){
 const auto* record=nativeModule(id);if(!record){error="Unknown native module registry entry";return nullptr;}
 auto path=std::filesystem::absolute(root/record->filename);
 HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(file==INVALID_HANDLE_VALUE){error=std::string("Required native module unavailable: ")+record->filename;return nullptr;}
 const bool valid=verify(file,record->sha256);HMODULE lib=nullptr;
 if(valid)lib=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
 CloseHandle(file);if(!valid||!lib)error=std::string(valid?"Cannot load verified native module: ":"Native SHA mismatch: ")+record->filename;
 return lib;
}
}
