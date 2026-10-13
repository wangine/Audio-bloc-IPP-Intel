#include "StageLinkClient.h"
#include "StageLinkServer.h"
#include "TestSupport.h"
#include <future>
#include <chrono>
using namespace entropy::link;
int main(){
 auto name=endpointName()+L".test."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(qpc());
 StageLinkClient client(name);StageLinkResponse response{};std::string error;auto r=makeRequest(StageLinkAction::Query);auto begin=qpc();CHECK(!client.transact(r,response,100,error));CHECK(qpc()-begin<frequency());
 CHECK(validRequest(r,GetCurrentProcessId()));auto bad=r;bad.magic++;CHECK(!validRequest(bad,GetCurrentProcessId()));bad=r;bad.bytes++;CHECK(!validRequest(bad,GetCurrentProcessId()));bad=r;std::memset(bad.nonce,0,16);CHECK(!validRequest(bad,GetCurrentProcessId()));bad=r;bad.version++;CHECK(!validRequest(bad,GetCurrentProcessId()));bad=r;bad.reserved[0]=1;CHECK(!validRequest(bad,GetCurrentProcessId()));CHECK(!validRequest(r,GetCurrentProcessId()+1));
 StageLinkServer server(name);CHECK(server.start(error));StageLinkServer second(name);CHECK(!second.start(error));
 auto rpc=std::async(std::launch::async,[&]{return client.transact(r,response,1000,error);});StageLinkRequest received{};uint32_t pid=0;begin=qpc();while(!server.tryTake(received,pid)){CHECK(qpc()-begin<frequency()*2);Sleep(1);}CHECK(pid==GetCurrentProcessId());CHECK(!std::memcmp(&r,&received,sizeof(r)));StageLinkResponse reply{};reply.sourcePid=GetCurrentProcessId();std::memcpy(reply.nonce,r.nonce,16);reply.status=StageLinkStatus::Waiting;auto stale=reply;stale.nonce[0]^=1;server.complete(stale);CHECK(rpc.wait_for(std::chrono::milliseconds(20))==std::future_status::timeout);server.complete(reply);CHECK(rpc.get());CHECK(response.status==StageLinkStatus::Waiting);CHECK(!std::memcmp(response.nonce,r.nonce,16));server.stop();
 std::cout<<"PASS local authenticated bounded control transport, malformed/PID rejection, nonce and single server owner\n";
}
