#include "IPC/Codec.h"
#include "IPC/Json.h"
#include <iostream>
#include <cmath>
#include <stdexcept>
using namespace entropy::ipc;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<class F> void rejected(F f,const char* label="invalid input accepted"){bool fail=false;try{f();}catch(const std::exception&){fail=true;}require(fail,label);}
int main(){try{
 Frame f{2,42,"{\"value\":-0.0}"};auto bytes=encode(f);Framer r;
 for(auto byte:bytes)r.push(std::span(&byte,1));auto frames=r.take();require(frames.size()==1&&frames[0].requestId==42&&frames[0].payload==f.payload,"fragmented frame");
 auto both=bytes;both.insert(both.end(),bytes.begin(),bytes.end());r.push(both);require(r.take().size()==2,"coalesced frames");
 auto broken=bytes;broken[0]=2;rejected([&]{Framer bad;bad.push(broken);});
 broken=bytes;broken[4]=0;broken[5]=0;broken[6]=0;broken[7]=0;rejected([&]{Framer bad;bad.push(broken);});
 rejected([&]{encode(Frame{1,1,std::string(kMaximumFrameBytes,'a')});});
 auto v=parse("{\"id\":\"18446744073709551615\",\"name\":\"DAC \\u00e2 \\uD83D\\uDE00\",\"samples\":[-0.0,0.12345678901234567,true,null]}");
 require(v.at("id").asString()=="18446744073709551615","exact uint64 string");require(std::signbit(v.at("samples").asArray()[0].asNumber()),"signed zero parse");
 auto round=parse(dump(v));require(std::signbit(round.at("samples").asArray()[0].asNumber()),"signed zero serialize");require(round.at("samples").asArray()[1].asNumber()==v.at("samples").asArray()[1].asNumber(),"FP64 roundtrip");
 for(auto invalid:{"NaN","Infinity","1e999","[1,]","{\"x\":1,\"x\":2}","01","\"\\uD800\"","true false","{\"x\":null"})rejected([&]{parse(invalid);});
 rejected([&]{parse(std::string(65,'[')+std::string(65,']'));},"excessive JSON depth accepted");
 std::cout<<"PASS Stage05 fragmented/coalesced framing, limits/version, strict JSON, UTF-8 and FP64/revision roundtrip\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
