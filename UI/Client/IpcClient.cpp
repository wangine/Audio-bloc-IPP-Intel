#include "IpcClient.h"
#include <QJsonDocument>
#include <QDateTime>
#include <QJsonArray>
#include <algorithm>
namespace entropy::ui {
IpcClient::~IpcClient(){watchdog_.stop();reconnect_.stop();QObject::disconnect(&socket_,nullptr,this,nullptr);socket_.abort();pending_.clear();}
IpcClient::IpcClient(QObject* parent):QObject(parent){
 connect(&socket_,&QLocalSocket::connected,this,[this]{reconnect_.stop();if(connectionChanged)connectionChanged(true);});
 connect(&socket_,&QLocalSocket::disconnected,this,[this]{fail("AudioCore disconnected; mutations are not replayed");});
 connect(&socket_,&QLocalSocket::readyRead,this,[this]{receive();});
 connect(&socket_,&QLocalSocket::errorOccurred,this,[this](auto){fail(socket_.errorString());});
 watchdog_.setInterval(100);connect(&watchdog_,&QTimer::timeout,this,[this]{auto now=QDateTime::currentMSecsSinceEpoch();for(const auto& [id,p]:pending_)if(now-p.started>p.budget){fail("AudioCore reply timeout");break;}});watchdog_.start();
 reconnect_.setInterval(500);connect(&reconnect_,&QTimer::timeout,this,[this]{if(socket_.state()==QLocalSocket::UnconnectedState)socket_.connectToServer(endpoint_);});
}
void IpcClient::connectToCore(const QString& endpoint){endpoint_=endpoint;socket_.connectToServer(endpoint);reconnect_.start();}
bool IpcClient::send(const QJsonObject& request,Reply reply){
 if(!connected()||pending_.size()>=64||socket_.bytesToWrite()>2*1024*1024)return false;
 auto data=QJsonDocument(request).toJson(QJsonDocument::Compact);if(data.size()>int(ipc::kMaximumFrameBytes-16))return false;
 if(++next_==0)++next_;ipc::Frame frame{2,next_,std::string(data.constData(),size_t(data.size()))};auto bytes=ipc::encode(frame);
 const auto action=request["action"].toString();
 bool scene=action=="program"||(action=="parameter"&&request["id"].toString()=="cinemaSource");
 if(action=="parameters")for(const auto& v:request["values"].toArray())if(v.toObject()["id"].toString()=="cinemaSource")scene=true;
 const bool preflight=scene||action=="arm"||action=="preflight-live";const bool queued=std::any_of(pending_.begin(),pending_.end(),[](const auto& p){return p.second.preflight;});if(preflight)for(auto& [id,p]:pending_)p.budget=30000;pending_.emplace(next_,Pending{std::move(reply),QDateTime::currentMSecsSinceEpoch(),preflight,(preflight||queued)?30000:5000});if(socket_.write(reinterpret_cast<const char*>(bytes.data()),qint64(bytes.size()))<0){pending_.erase(next_);return false;}return true;
}
void IpcClient::receive(){try{auto bytes=socket_.readAll();framer_.push(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes.constData()),size_t(bytes.size())));for(auto& frame:framer_.take()){
 auto p=pending_.find(frame.requestId);if(p==pending_.end())throw std::runtime_error("Uncorrelated AudioCore reply");QJsonParseError error;auto document=QJsonDocument::fromJson(QByteArray::fromStdString(frame.payload),&error);if(error.error!=QJsonParseError::NoError||!document.isObject())throw std::runtime_error("Malformed AudioCore JSON");auto callback=std::move(p->second.reply);pending_.erase(p);if(callback)callback(document.object());
 }}catch(const std::exception& e){fail(QString::fromUtf8(e.what()));}}
void IpcClient::fail(const QString& error){if(failing_)return;failing_=true;auto pending=std::move(pending_);pending_.clear();framer_=ipc::Framer{};socket_.abort();if(connectionChanged)connectionChanged(false);if(fault)fault(error);for(auto& [id,p]:pending)if(p.reply)p.reply(QJsonObject{{"ok",false},{"error",error}});if(!endpoint_.isEmpty())reconnect_.start();failing_=false;}
}
