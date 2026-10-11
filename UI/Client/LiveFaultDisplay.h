#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
namespace entropy::ui {
inline QString liveFaultReason(unsigned faults){QStringList reasons;
 if(faults&1)reasons<<"OVERFLOW";if(faults&2)reasons<<"STARVATION";if(faults&4)reasons<<"CLOCK";
 if(faults&8)reasons<<"PCM";if(faults&16)reasons<<"DEVICE";if(faults&32)reasons<<"INPUT";
 if(reasons.empty())return "SINK";return reasons.join(" / ");
}
inline QString liveFaultDisplay(const QJsonObject& v){unsigned faults=0;
 for(auto domain:v["domains"].toArray())faults|=unsigned(domain.toObject()["faults"].toInt());
 const auto state=v["lifecycleStatus"].toString();
 return state+"; "+liveFaultReason(faults)+(state=="DISARMED"?"; explicit ARM to resume":"; output closing");
}
}
