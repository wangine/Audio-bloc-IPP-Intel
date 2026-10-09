#include "Client/EngineFacade.h"
#include "Legacy/RenderWindow.h"
#include <QApplication>
#include <QTableWidget>
#include <QPushButton>
#include <QComboBox>
#include <QDialog>
#include <QTimer>
#include <QJsonArray>
#include <iostream>
int main(int argc,char** argv){
 QApplication app(argc,argv);entropy::ui::EngineFacade facade("EntropyAudioCore.Stage07.NoHardware");entropy::stage::RenderWindow window(facade);window.show();window.openModule(entropy::stage::RenderPage::Devices);
 auto* page=window.moduleDialog(entropy::stage::RenderPage::Devices);bool pass=true;
 auto format=[](int channels){return QJsonArray{QJsonObject{{"rate",192000},{"channels",channels},{"containerBits",32},{"validBits",24},{"encoding",1}}};};
 QJsonArray inventory{QJsonObject{{"endpoint","fixture-A"},{"name","FIRST ENDPOINT"},{"active",true},{"maximumChannels",2},{"formats",format(2)}},QJsonObject{{"endpoint","fixture-B"},{"name","SECOND ENDPOINT"},{"active",true},{"maximumChannels",8},{"formats",format(8)}}};
 QJsonObject cache{{"devices",inventory},{"bindings",QJsonArray{QJsonValue::Null,QJsonObject{{"endpoint","fixture-B"},{"resolvedEndpoint","fixture-B"},{"logicalId","DAC_B"},{"name","SECOND ENDPOINT"},{"state",1},{"approvalRevision","42"}}}},{"configuration",QJsonArray{}}};
 facade.updateDevices(QJsonObject{{"ok",true},{"data",cache}});
 QTimer::singleShot(800,&app,[&]{auto* table=page->findChild<QTableWidget*>("deviceInventory");if(!table||table->rowCount()!=2){app.exit(1);return;}table->selectRow(1);
  QTimer::singleShot(900,&app,[&,table]{pass=pass&&table->currentRow()==1;std::cout<<"refresh row="<<table->currentRow()<<"\n";
   // Inventory reordering must preserve endpoint identity rather than row index.
   auto list=cache["devices"].toArray();QJsonArray swapped{list[1],list[0]};cache["devices"]=swapped;facade.updateDevices(QJsonObject{{"ok",true},{"data",cache}});
   QTimer::singleShot(700,&app,[&,table]{pass=pass&&table->currentRow()==0&&table->item(0,2)->text()=="SECOND ENDPOINT";auto* button=page->findChild<QPushButton*>("configureDacB");if(!button||!button->isEnabled()){app.exit(1);return;}
    QTimer::singleShot(100,&app,[&]{auto* modal=qobject_cast<QDialog*>(QApplication::activeModalWidget());bool exact=false;if(modal)for(auto* combo:modal->findChildren<QComboBox*>())for(int i=0;i<combo->count();++i){auto data=combo->itemData(i).toULongLong();if((data>>32)==192000&&((data>>16)&0xffff)==8)exact=true;}pass=pass&&exact;std::cout<<"exact second endpoint format="<<exact<<"\n";if(modal)modal->reject();});button->click();
    std::cout<<(pass?"PASS Qt selected endpoint survives refresh/reorder; DAC B exact formats from resolved identity; IPC only\n":"FAIL Qt endpoint identity\n");app.exit(pass?0:1);
   });
  });
 });QTimer::singleShot(6000,&app,[&]{app.exit(1);});return app.exec();
}
