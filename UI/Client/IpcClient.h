#pragma once
#include <QLocalSocket>
#include <QTimer>
#include <QJsonObject>
#include <functional>
#include <map>
#include "IPC/Codec.h"
namespace entropy::ui {
class IpcClient final:public QObject {
public:
 using Reply=std::function<void(const QJsonObject&)>;
 explicit IpcClient(QObject* parent=nullptr);
 ~IpcClient();
 void connectToCore(const QString& endpoint);
 bool send(const QJsonObject&,Reply);
 bool connected()const{return socket_.state()==QLocalSocket::ConnectedState;}
 std::function<void(bool)> connectionChanged;
 std::function<void(QString)> fault;
private:
 struct Pending{Reply reply;qint64 started;bool preflight=false;qint64 budget=5000;};
 void receive();void fail(const QString&);
 QLocalSocket socket_;QTimer watchdog_,reconnect_;QString endpoint_;
 ipc::Framer framer_;uint32_t next_=0;std::map<uint32_t,Pending> pending_;bool failing_=false;
};
}
