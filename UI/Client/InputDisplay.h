#pragma once
#include <QJsonObject>
#include <QString>
namespace entropy::ui {
struct InputDisplay {unsigned channels=0,layout=0;bool receiving=false;QString state="WAITING",label="CINEMA WAITING",endpoint,session;};
inline InputDisplay inputDisplay(const QJsonObject& v,bool cinema) {InputDisplay out;out.label=cinema?"CINEMA WAITING":"MUSIC WAITING";if(v["inputContractVersion"].toInt()!=1)return out;auto s=v["inputs"].toObject()[cinema?"cinema":"music"].toObject();out.channels=s["channels"].toInt();out.layout=s["layout"].toInt();out.state=s["state"].toString("WAITING");out.receiving=out.state=="RECEIVING";out.endpoint=s["endpoint"].toString();out.session=s["sourceSession"].toString();bool contract=cinema?((out.channels==6&&out.layout==5)||(out.channels==8&&out.layout==6)):(out.channels==3&&out.layout==4);out.receiving&=contract;out.label=out.receiving?(cinema?(out.channels==6?"CINEMA 5.1":"CINEMA 7.1"):"MUSIC L/R/C"):(cinema?"CINEMA ":"MUSIC ")+out.state;return out;}
}
