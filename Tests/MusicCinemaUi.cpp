#include "Client/InputDisplay.h"
#include "Client/LiveFaultDisplay.h"
#include <iostream>
int main(){using namespace entropy::ui;for(auto channels:{3,6,8}){auto root=QJsonObject{{"inputContractVersion",1},{"inputs",QJsonObject{{channels==3?"music":"cinema",QJsonObject{{"channels",channels},{"layout",channels==3?4:channels==6?5:6},{"state","RECEIVING"},{"endpoint","local"},{"sourceSession","uuid"}}}}}};auto v=inputDisplay(root,channels!=3);if(v.channels!=unsigned(channels)||!v.receiving||v.endpoint!="local"||v.session!="uuid"){std::cerr<<"FAIL typed source presentation\n";return 1;}if(channels==6&&!v.label.contains("5.1"))return 2;if(channels==8&&!v.label.contains("7.1"))return 3;}
 auto absent=inputDisplay({},true);if(absent.receiving||!absent.label.contains("WAITING"))return 4;
 auto unknown=inputDisplay(QJsonObject{{"inputContractVersion",2}},true);if(unknown.receiving)return 5;
 for(unsigned fault:{2u,4u,8u,16u,32u}){
  auto root=QJsonObject{{"lifecycleStatus","DISARMED"},{"domains",QJsonArray{QJsonObject{{"faults",int(fault)},{"sinkFaults",1}}}}};
  const auto display=liveFaultDisplay(root);
  if(!display.contains("DISARMED")||!display.contains("explicit ARM")||display.contains("OVERFLOW"))return 6;
  root["lifecycleStatus"]="STOPPING";if(liveFaultDisplay(root).contains("explicit ARM"))return 7;
 }
 if(!liveFaultReason(2).contains("STARVATION")||!liveFaultReason(4).contains("CLOCK"))return 8;
 std::cout<<"PASS Music3/Cinema6/8 presentation, absent/legacy telemetry, retained fault and direct manual ARM presentation\n";
}
