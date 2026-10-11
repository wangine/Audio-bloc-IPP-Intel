#include "DspState.h"
#include "TestSupport.h"
#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QTemporaryDir>
int main(int argc,char**argv){
 QCoreApplication app(argc,argv);QTemporaryDir tmp;auto root=std::filesystem::path(tmp.path().toStdWString());std::string error;
 DspState original;original.centerConfigured=true;CHECK(original.save(root/"input.Edsp",error));
 QFile f(QString::fromStdWString((root/"input.Edsp").wstring()));CHECK(f.open(QIODevice::ReadOnly));auto object=QJsonDocument::fromJson(f.readAll()).object();f.close();
 QJsonArray bands;for(int i=0;i<8;i++)bands.append(QJsonObject{{"enabled",true},{"filter_type",0},{"detector_mode",1},{"detector_source",0},{"dynamics_mode",0},{"frequency_hz",1000.+i*100},{"q",1.},{"static_gain_db",i==0?6.:0.},{"threshold_db",-24.},{"ratio",2.},{"knee_db",6.},{"attack_ms",20.},{"release_ms",180.},{"max_cut_db",-12.},{"max_boost_db",6.}});
 QJsonObject eq{{"mode",1},{"bands",bands}};object.insert("parametric_eq_settings",eq);CHECK(f.open(QIODevice::WriteOnly));CHECK(f.write(QJsonDocument(object).toJson())>0);f.close();
 DspState loaded;CHECK(loaded.load(root/"input.Edsp",error));CHECK(loaded.save(root/"roundtrip.Edsp",error));
 QFile result(QString::fromStdWString((root/"roundtrip.Edsp").wstring()));CHECK(result.open(QIODevice::ReadOnly));auto saved=QJsonDocument::fromJson(result.readAll()).object();CHECK(saved.value("parametric_eq_settings").toObject()==eq);
 object["parametric_eq_settings"]=QJsonObject{{"mode",1},{"bands",QJsonArray{}}};CHECK(f.open(QIODevice::WriteOnly));CHECK(f.write(QJsonDocument(object).toJson())>0);f.close();CHECK(!loaded.load(root/"input.Edsp",error));
 std::cout<<"PASS local EQ .Edsp settings roundtrip; malformed settings rejected\n";
}
