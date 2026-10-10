#include "IPC/CommandServer.h"
#include <iostream>
#include <atomic>
#include <array>
using namespace entropy::ipc;
int main()try{
 CommandServer server;std::string error;std::atomic<bool> closed{false};
 const auto endpoint=L"EntropyAudioCore.Stage017.Writer."+std::to_wstring(GetCurrentProcessId());
 if(!server.start(endpoint,[&](const Frame& request){
  if(request.requestId==1)return Frame{2,1,std::string(250000,'x')};
  closed=true;return Frame{2,request.requestId,"DISARM ADMITTED"};
 },{},error))throw std::runtime_error(error);
 auto client=CreateFileW(CommandServer::pipePath(endpoint).c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);
 if(client==INVALID_HANDLE_VALUE)throw std::runtime_error("connect");
 DWORD count=0;auto send=[&](uint32_t id){auto bytes=encode({2,id,id==1?"{\"action\":\"telemetry\"}":"{\"action\":\"disarm\"}"});if(!WriteFile(client,bytes.data(),DWORD(bytes.size()),&count,nullptr))throw std::runtime_error("send");};
 send(1);Sleep(20);const auto begin=GetTickCount64();send(2);
 while(!closed&&GetTickCount64()-begin<1000)Sleep(1);
 const auto admission=GetTickCount64()-begin;if(!closed||admission>=50)throw std::runtime_error("DISARM reader blocked behind unread FFT response");
 // Read both responses afterwards: only one writer and both identities survive.
 auto receive=[&]{std::array<unsigned char,16> header{};size_t done=0;while(done<16){if(!ReadFile(client,header.data()+done,DWORD(16-done),&count,nullptr)||!count)throw std::runtime_error("header");done+=count;}
  const auto size=load32(header.data()+4);std::vector<unsigned char> bytes(header.begin(),header.end());bytes.resize(size);while(done<size){if(!ReadFile(client,bytes.data()+done,DWORD(size-done),&count,nullptr)||!count)throw std::runtime_error("body");done+=count;}Framer reader;reader.push(bytes);return reader.take()[0];};
 const auto first=receive(),second=receive();if(first.requestId!=1||first.payload.size()!=250000||second.requestId!=2)throw std::runtime_error("correlation/single writer");
 CloseHandle(client);server.stop();std::cout<<"PASS unread 250k FFT response; DISARM admission "<<admission<<" ms; correlated single writer\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
