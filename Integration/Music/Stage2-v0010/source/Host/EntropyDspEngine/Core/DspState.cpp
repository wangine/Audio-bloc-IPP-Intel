#include "DspState.h"
#include "DspEqState.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFileInfo>
#include <cstring>
namespace entropy::stage {
namespace {
bool centerObject(const QJsonObject& object,CenterSettings& settings){
 if(object.size()!=16)return false;
 for(unsigned i=0;i<16;i++){const auto value=object.value(centerIds[i]);if(!value.isDouble())return false;settings.values[i]=value.toDouble();}
 return validCenterSettings(settings);
}
// First activation reuses the archived Center V2 parameters. Legacy limiter values
// are intentionally not part of this control schema; the native worker forces OFF.
bool archivedCenter(const QByteArray& project,CenterSettings& settings,std::string& error){
 auto document=QJsonDocument::fromJson(project);if(!document.isObject())return true;
 for(const auto module:document.object().value("modules").toArray()){
  auto item=module.toObject();if(item.value("instance_id").toString()!="center.music")continue;
  if(item.value("module_id").toString()!="audio.entropy.center-extractor.fp64-v2"){error="Archived Center is not the accepted V2";return false;}
  auto bytes=QByteArray::fromBase64(item.value("serialized_state").toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
  if(!bytes.startsWith("ENTROPY_CENTER_V2_STATE=2\n")){error="Invalid archived Center V2 state";return false;}
  QJsonObject values;
  for(const auto& line:bytes.split('\n')){const auto separator=line.indexOf('=');if(separator<=0)continue;auto id=line.left(separator);for(auto expected:centerIds)if(id==expected){bool ok=false;double value=line.mid(separator+1).toDouble(&ok);if(!ok){error="Invalid archived Center parameter";return false;}values.insert(QString::fromLatin1(id),value);}}
  if(!centerObject(values,settings)){error="Incomplete/out-of-range archived Center parameters";return false;}return true;
 }return true;
}
}
bool DspState::load(const std::filesystem::path& p,std::string& error){
 QFile f(QString::fromStdWString(p.wstring()));if(!f.open(QIODevice::ReadOnly)||f.size()>16*1024*1024){error="State missing/oversized";return false;}
 QJsonParseError e;auto doc=QJsonDocument::fromJson(f.readAll(),&e);auto o=doc.object();auto mode=o.value("mode").toString();
 if(e.error!=QJsonParseError::NoError || (mode!="BYPASS_TRANSPARENT"&&mode!="CENTER_V2_C_ONLY")){error="Unsupported Stage2 state";return false;}
 DspState result;result.importedProject=QByteArray::fromBase64(o.value("r10_project_base64").toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);result.importedIni=QByteArray::fromBase64(o.value("r10_ini_base64").toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);result.sourceDiscovery=o.value("source_discovery").toString();
 result.centerConfigured=mode=="CENTER_V2_C_ONLY";
 if(result.centerConfigured){if(!o.value("center_settings").isObject() || !centerObject(o.value("center_settings").toObject(),result.centerSettings)){error="Invalid Center state parameters";return false;}}
 else if(!archivedCenter(result.importedProject,result.centerSettings,error))return false;
 if(o.contains("parametric_eq_settings")&&(!o.value("parametric_eq_settings").isObject()||!eqFromJson(o.value("parametric_eq_settings").toObject(),result.eqSettings))){error="Invalid EQ profile settings";return false;}
 if(o.contains("lrc_alignment_active")){if(!o.value("lrc_alignment_active").isBool()){error="Invalid LRC alignment profile setting";return false;}result.alignmentActive=o.value("lrc_alignment_active").toBool();}
 *this=std::move(result);return true;
}
bool DspState::save(const std::filesystem::path& p,std::string& error) const{
 if(!validCenterSettings(centerSettings)){error="Invalid Center parameters";return false;}
 std::error_code directoryError;if(!p.parent_path().empty())std::filesystem::create_directories(p.parent_path(),directoryError);if(directoryError){error="Cannot create profile directory: "+directoryError.message();return false;}QJsonObject o{{"schema",centerConfigured?2:1},{"mode",centerConfigured?"CENTER_V2_C_ONLY":"BYPASS_TRANSPARENT"},{"cx_active",centerConfigured && centerSettings.values[15]!=0},{"dd_reserved",true},{"pdc_active",false},{"source_discovery",sourceDiscovery},{"r10_project_base64",QString::fromLatin1(importedProject.toBase64())},{"r10_ini_base64",QString::fromLatin1(importedIni.toBase64())}};
 if(centerConfigured){QJsonObject values;for(unsigned i=0;i<16;i++)values.insert(centerIds[i],centerSettings.values[i]);o.insert("center_settings",values);o.insert("algorithmic_latency_samples",QJsonValue());}
 else o.insert("algorithmic_latency_samples",0);
 if(!validEqSettings(eqSettings)){error="Invalid EQ profile settings";return false;}o.insert("parametric_eq_settings",eqToJson(eqSettings));
 if(alignmentActive.has_value())o.insert("lrc_alignment_active",*alignmentActive);
 auto data=QJsonDocument(o).toJson();QSaveFile f(QString::fromStdWString(p.wstring()));if(!f.open(QIODevice::WriteOnly)||f.write(data)!=data.size()||!f.commit()){error="Atomic state save failed";return false;}return true;
}
bool DspState::loadStartup(const std::filesystem::path& root,QString& selected,std::string& warning,std::string& error){
 selected.clear();warning.clear();error.clear();
 const auto cache=root/"state"/"Stage2State.json",selection=root/"state"/"LastProfile.json";
 QFile file(QString::fromStdWString(selection.wstring()));
 if(file.exists()){
  if(file.open(QIODevice::ReadOnly)&&file.size()<=32768){
   QJsonParseError parse;const auto document=QJsonDocument::fromJson(file.readAll(),&parse);const auto object=document.object();
   const auto path=object.value("path").toString();
   if(parse.error==QJsonParseError::NoError&&object.value("schema").toInt()==1&&!path.isEmpty()&&QFileInfo(path).isAbsolute()){
    selected=path;DspState next;std::string reason;
    if(next.load(std::filesystem::path(path.toStdWString()),reason)){*this=std::move(next);return true;}
    warning="Last profile unavailable: "+path.toStdString()+" ("+reason+"). Using startup cache.";
   }else warning="Invalid last-profile selection. Using startup cache.";
  }else warning="Last-profile selection unreadable/oversized. Using startup cache.";
 }
 QFile cached(QString::fromStdWString(cache.wstring()));
 if(cached.exists())return load(cache,error);
 *this=DspState{};if(!warning.empty())warning+=" No startup cache exists; using defaults.";return true;
}
bool DspState::remember(const std::filesystem::path& root,const QString& selected,std::string& error) const{
 if(selected.isEmpty()){error="Empty profile path";return false;}
 if(!save(root/"state"/"Stage2State.json",error))return false;
 const auto data=QJsonDocument(QJsonObject{{"schema",1},{"path",QFileInfo(selected).absoluteFilePath()}}).toJson();
 QSaveFile file(QString::fromStdWString((root/"state"/"LastProfile.json").wstring()));
 if(!file.open(QIODevice::WriteOnly)||file.write(data)!=data.size()||!file.commit()){error="Startup cache saved, but last-profile path could not be saved";return false;}
 return true;
}
}
