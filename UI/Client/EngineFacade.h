#pragma once
#include "IpcClient.h"
#include "Legacy/RenderUiBridge.h"
#include <QJsonArray>
#include <memory>
namespace entropy::ui {
class EngineFacade final:public QObject {
public:
 explicit EngineFacade(const QString& endpoint,QObject* parent=nullptr);
 ~EngineFacade();
 stage::RenderUiBridge& bridge(){return bridge_;}
 bool connected() const{return client_.connected();}
 uint64_t blocks() const{return telemetry_->heartbeat;}
 uint64_t faults() const{return telemetry_->outputFaultLatched;}
 uint64_t lastBlockQpc() const {return lastQpc_;}
 bool submit(const host::EngineControlSnapshot&,uint64_t&);
 bool request(QJsonObject,IpcClient::Reply={});
 const QJsonObject& devices()const{return devices_;}
 bool profileRequired() const{return metadata_["profileRequired"].toBool();}
 QString profilePath() const{return metadata_["profilePath"].toString();}
 QString profileError() const{return metadata_["profileError"].toString();}
 void updateDevices(const QJsonObject& reply);
 static EngineFacade* current();
private:
 void poll();void receive(const QJsonObject&);
 IpcClient client_;QTimer poll_;stage::RenderUiBridge bridge_;
 std::unique_ptr<host::EngineTelemetrySnapshot> telemetry_=std::make_unique<host::EngineTelemetrySnapshot>();
 QJsonObject metadata_,devices_;uint64_t sequence_=0,lastQpc_=0,session_=0;bool polling_=false;QString lastError_;
};
}
