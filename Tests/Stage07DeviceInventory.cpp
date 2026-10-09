#include "Client/EngineFacade.h"
#include "Legacy/RenderWindow.h"
#include <QApplication>
#include <QJsonArray>
#include <QTableWidget>
#include <QTimer>
#include <QPushButton>
#include <QFontDatabase>
#include <QScrollBar>
#include <iostream>

// Read-only against a real Core, or deterministic inventory presentation fixture.
int main(int argc,char** argv){
 QApplication app(argc,argv);
 QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
 QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeuisb.ttf");
 const bool real=argc>1;
 entropy::ui::EngineFacade facade(real?QString::fromUtf8(argv[1]):"EntropyAudioCore.InventoryFixture");
 entropy::stage::RenderWindow window(facade);window.show();window.openModule(entropy::stage::RenderPage::Devices);
 auto* page=window.moduleDialog(entropy::stage::RenderPage::Devices);
 if(!real){
  QJsonArray list;
  for(int i=0;i<20;++i)list.append(QJsonObject{{"endpoint",QString("offline-%1").arg(i)},{"name",QString("Disconnected %1").arg(i)},{"active",false}});
  list.append(QJsonObject{{"endpoint","dac-A"},{"name","Connected DAC A"},{"active",true},{"maximumChannels",8}});
  list.append(QJsonObject{{"endpoint","dac-B"},{"name","Connected DAC B"},{"active",true},{"maximumChannels",8}});
  facade.updateDevices(QJsonObject{{"ok",true},{"data",QJsonObject{{"devices",list},{"bindings",QJsonArray{QJsonValue::Null,QJsonValue::Null}},{"configuration",QJsonArray{}}}}});
 }
 QTimer::singleShot(1200,&app,[&]{
  auto* table=page->findChild<QTableWidget*>("deviceInventory");
  if(!table||table->rowCount()==0){std::cerr<<"FAIL inventory empty\n";app.exit(1);return;}
  std::cout<<"Inventory cache="<<facade.devices()["devices"].toArray().size()<<" table="<<table->rowCount()<<"\n";
  bool activeSeen=false,offlineSeen=false,ordered=true,namesVisible=true;
  for(int i=0;i<table->rowCount();++i){
   const bool active=table->item(i,0)->text()=="ONLINE";
   if(active){activeSeen=true;if(offlineSeen)ordered=false;namesVisible&=table->columnWidth(2)>=table->fontMetrics().horizontalAdvance(table->item(i,2)->text())+12;}else offlineSeen=true;
   std::cout<<i<<" "<<table->item(i,0)->text().toStdString()<<" "<<table->item(i,2)->text().toStdString()<<"\n";
  }
  if(real&&argc>2)page->grab().save(QString::fromUtf8(argv[2]));
  std::cout<<(namesVisible?"PASS connected endpoint names fit inventory column\n":"FAIL connected endpoint names truncated\n");
  const bool orderedPass=activeSeen&&ordered&&(!real||facade.connected());
  std::cout<<(orderedPass?"PASS active connected endpoints visible before retained offline records; profile-independent IPC inventory\n":"FAIL connected endpoints hidden behind retained offline records\n");
  auto* bar=table->verticalScrollBar();bar->setValue(bar->maximum());const int position=bar->value();
  for(auto* button:page->findChildren<QPushButton*>())if(button->text()=="REFRESH DEVICES")button->click();
  QTimer::singleShot(800,&app,[&,table,position,orderedPass,namesVisible]{
   const bool scrollPass=position>0&&table->verticalScrollBar()->value()==position;
   std::cout<<"Scroll requested="<<position<<" after periodic refresh="<<table->verticalScrollBar()->value()<<"\n";
   std::cout<<(scrollPass?"PASS user scroll survives periodic inventory refresh\n":"FAIL periodic refresh cancels user scroll\n");
   app.exit(orderedPass&&namesVisible&&scrollPass?0:1);
  });
 });
 QTimer::singleShot(5000,&app,[&]{app.exit(1);});return app.exec();
}
