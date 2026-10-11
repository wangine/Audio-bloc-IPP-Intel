#pragma once
#include "InputContract.h"
#include "IPC/Json.h"
namespace entropy::input {
struct Configuration {
 std::wstring music=defaultEndpoint(Domain::Music),cinema=defaultEndpoint(Domain::Cinema);
 static Configuration decode(const ipc::Value& document){Configuration c;if(!document.contains("stage3_inputs"))return c;const auto& v=document.at("stage3_inputs");if(v.at("version").asUnsigned()!=1)throw std::invalid_argument("Input profile version incompatible");auto m=v.at("music").asString(),s=v.at("cinema").asString();c.music.assign(m.begin(),m.end());c.cinema.assign(s.begin(),s.end());if(!localEndpoint(c.music)||!localEndpoint(c.cinema))throw std::invalid_argument("Profile inputs must be local named pipes");return c;}
 ipc::Value encode() const {return ipc::Value::Object{{"version",1},{"music",std::string(music.begin(),music.end())},{"cinema",std::string(cinema.begin(),cinema.end())}};}
};
}
