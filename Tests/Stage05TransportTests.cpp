#include "IPC/CommandServer.h"
#include <iostream>
#include <stdexcept>
#include <array>
#include <atomic>
using namespace entropy::ipc;
void require(bool b,const char* message){if(!b)throw std::runtime_error(message);}
int main(){try{
 CommandServer server;std::atomic<unsigned> disconnects=0;std::string error;
 std::wstring endpoint=L"EntropyAudioCore.Test.Transport."+std::to_wstring(GetCurrentProcessId());
 require(server.start(endpoint,[](const Frame& f){return f;},[&]{disconnects.fetch_add(1);},error),error.c_str());
 CommandServer duplicate;require(!duplicate.start(endpoint,[](const Frame& f){return f;},{},error),"second owner admitted");
 auto path=CommandServer::pipePath(endpoint);HANDLE client=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);require(client!=INVALID_HANDLE_VALUE,"client connection");
 Frame request{1,71,"{\"revision\":\"9007199254740993\"}"};auto bytes=encode(request);DWORD count=0;
 for(auto& byte:bytes)require(WriteFile(client,&byte,1,&count,nullptr)&&count==1,"fragmented send");
 std::array<unsigned char,16> header{};size_t offset=0;
 while(offset<header.size()){require(ReadFile(client,header.data()+offset,DWORD(header.size()-offset),&count,nullptr)&&count,"header read");offset+=count;}
 unsigned size=load32(header.data()+4);std::vector<unsigned char> data(header.begin(),header.end());data.resize(size);offset=16;
 while(offset<size){require(ReadFile(client,data.data()+offset,DWORD(size-offset),&count,nullptr)&&count,"body read");offset+=count;}
 Framer parser;parser.push(data);auto responses=parser.take();require(responses.size()==1&&responses[0].payload==request.payload&&responses[0].requestId==71,"transport payload/correlation");
 Sleep(5500);require(disconnects.load()==1,"idle controlling peer lease was not revoked");
 CloseHandle(client);server.stop();require(disconnects.load()==1,"disconnect callback");
 require(server.start(endpoint,[](const Frame& f){return f;},{},error),"restart ownership");server.stop();
 require(server.start(endpoint,[&](const Frame& f){server.requestStopAfterReply();return f;},{},error),"shutdown drain ownership");
 client=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);require(client!=INVALID_HANDLE_VALUE,"shutdown drain connection");
 bytes=encode(request);require(WriteFile(client,bytes.data(),DWORD(bytes.size()),&count,nullptr)&&count==bytes.size(),"shutdown drain request");
 const unsigned char extra=0;require(WriteFile(client,&extra,1,&count,nullptr)&&count==1,"pipelined byte");
 Sleep(100);require(!server.shutdownReplyComplete(),"Inbound byte falsely acknowledged unread shutdown response");
 offset=0;while(offset<16){require(ReadFile(client,header.data()+offset,DWORD(16-offset),&count,nullptr)&&count,"delayed shutdown header");offset+=count;}
 size=load32(header.data()+4);data.assign(header.begin(),header.end());data.resize(size);offset=16;
 while(offset<size){require(ReadFile(client,data.data()+offset,DWORD(size-offset),&count,nullptr)&&count,"delayed shutdown body");offset+=count;}
 parser.push(data);responses=parser.take();require(responses.size()==1&&responses[0].payload==request.payload,"Shutdown reply lost before delayed read");
 CloseHandle(client);const auto deadline=GetTickCount64()+1000;while(!server.shutdownReplyComplete()&&GetTickCount64()<deadline)Sleep(1);require(server.shutdownReplyComplete(),"Shutdown EOF did not finish");server.stop();
 std::cout<<"PASS Stage05 same-user local pipe, exclusive owner, fragmented request/correlated reply, disconnect and shutdown/restart\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
