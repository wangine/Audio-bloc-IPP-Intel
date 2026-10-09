#include "StageLinkClient.h"
#include <iostream>
using namespace entropy::link;
int main(int argc,char** argv){if(argc!=3)return 2;std::string name=argv[1];StageLinkClient client(std::wstring(name.begin(),name.end()));std::string error;StageLinkResponse response{};if(!client.transact(makeRequest(StageLinkAction::Query),response,1000,error))return 3;
 const bool fault=std::string(argv[2])=="fault";if(fault && response.status!=StageLinkStatus::Fault)return 4;
 auto request=makeRequest(StageLinkAction::Attach);std::memcpy(request.expectedSession,response.session.session_id,16);if(!client.transact(request,response,1000,error))return 5;
 if(fault)return response.status==StageLinkStatus::Ok?6:0;return response.status==StageLinkStatus::Busy?0:7;
}
