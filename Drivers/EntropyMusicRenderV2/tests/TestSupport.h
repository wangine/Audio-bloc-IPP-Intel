#pragma once
#include "../model/Format.h"
#include <stdexcept>
#include <cstdio>
#include <cstring>
#define CHECK(x) do {if(!(x)) throw std::runtime_error(#x);} while(0)
inline entropy::music::PcmFormat format(uint32_t rate=44100,uint32_t bits=16) {
    entropy::music::PcmFormat f{64,rate,2,bits,bits==16?16u:24u,bits/4,rate*(bits/4),3,0xfffe,1,{},{}};
    std::memcpy(f.subFormat,entropy::music::PcmGuid,16); return f;
}
struct Case {const char* name;void(*run)();};
inline int runTests(const Case* tests,size_t count) {
    int fails=0;
    for(size_t i=0;i<count;++i) {try{tests[i].run();std::printf("PASS %s\n",tests[i].name);}catch(const std::exception& e){++fails;std::printf("FAIL %s: %s\n",tests[i].name,e.what());}}
    std::printf("%zu cases, %d failures\n",count,fails);return fails?1:0;
}
