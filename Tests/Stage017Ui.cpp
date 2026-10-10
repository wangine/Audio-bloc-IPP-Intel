#include "Client/EngineFacade.h"
#include <QApplication>
#include <QLocalServer>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QThread>
#include <iostream>
int main(int argc,char** argv){
 QApplication app(argc,argv);QLocalServer server;const auto endpoint="EntropyAudioCore.Stage017.Ui."+QString::number(QCoreApplication::applicationPid());
 if(!server.listen(endpoint))return 1;
 bool ack=false,callback=false,allowCompletion=false,completionPolled=false;QLocalSocket* peer=nullptr;entropy::ipc::Framer reader;
 QObject::connect(&server,&QLocalServer::newConnection,[&]{auto* socket=server.nextPendingConnection();peer=socket;
  QObject::connect(socket,&QLocalSocket::readyRead,[&,socket]{auto bytes=socket->readAll();reader.push(std::span(reinterpret_cast<const unsigned char*>(bytes.constData()),size_t(bytes.size())));
   for(const auto& frame:reader.take()){
    auto request=QJsonDocument::fromJson(QByteArray::fromStdString(frame.payload)).object();auto action=request["action"].toString();QJsonObject data;
    if(action=="save-profile"){ack=true;data={{"status","ACCEPTED"},{"commandId","77"}};}
    else if(action=="command-results"){completionPolled=true;data={{"results",QJsonArray{QJsonObject{{"commandId","77"},{"complete",allowCompletion},{"reply",QJsonObject{{"ok",true},{"data",QJsonObject{{"status","SAVED"}}}}}}}}};}
    auto response=QJsonDocument(QJsonObject{{"ok",true},{"session","1"},{"data",data}}).toJson(QJsonDocument::Compact);auto out=entropy::ipc::encode({frame.command,frame.requestId,response.toStdString()});socket->write(reinterpret_cast<const char*>(out.data()),qint64(out.size()));
   }
  });
 });
 entropy::ui::EngineFacade facade(endpoint);
 auto until=[&](auto predicate,int timeout){QElapsedTimer t;t.start();while(!predicate()&&t.elapsed()<timeout){app.processEvents();QThread::msleep(1);}return predicate();};
 if(!until([&]{return facade.connected();},1000))return 2;
 facade.request({{"action","save-profile"},{"path","offline-fixture.entropy"}},[&](const QJsonObject& result){callback=result["data"].toObject()["status"].toString()=="SAVED";});
 if(!until([&]{return ack;},1000)||callback){std::cerr<<"FAIL admission mislabeled as saved\n";return 3;}
 allowCompletion=true;if(!until([&]{return callback;},1000)){std::cerr<<"FAIL correlated completion not delivered\n";return 4;}
 allowCompletion=false;completionPolled=false;bool disconnectedCallback=false;
 facade.request({{"action","save-profile"},{"path","offline-fixture.entropy"}},[&](const QJsonObject& result){disconnectedCallback=!result["ok"].toBool()&&result["status"].toString()=="COMPLETION_UNKNOWN";});
 if(!until([&]{return completionPolled;},1000))return 5;
 peer->abort();if(!until([&]{return disconnectedCallback;},1000)){std::cerr<<"FAIL accepted callback lost on disconnect\n";return 6;}
 std::cout<<"PASS UI waits for completion and settles accepted callbacks on disconnect\n";return 0;
}
