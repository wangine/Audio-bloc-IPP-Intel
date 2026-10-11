#include "DspSessionDiscovery.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <cstring>
namespace entropy::stage {
bool DspSessionDiscovery::readExternal(const std::filesystem::path& path,StageBusSessionHeader& s,std::string& error){
 QFile f(QString::fromStdWString(path.wstring()));if(!f.open(QIODevice::ReadOnly)||f.size()>8192){error="Missing/oversized session discovery";return false;}
 QJsonParseError parse{};auto document=QJsonDocument::fromJson(f.readAll(),&parse);
 if(parse.error!=QJsonParseError::NoError || !document.isObject() || document.object().value("owner").toString()!="EXTERNAL"){error="Source must publish EXTERNAL ownership; no automatic takeover";return false;}
 auto encoded=document.object().value("session_header_base64").toString().toLatin1();
 auto data=QByteArray::fromBase64(encoded,QByteArray::AbortOnBase64DecodingErrors);
 if(data.size()!=256 || data.toBase64()!=encoded){error="Invalid canonical session header";return false;}
 std::memcpy(&s,data.constData(),256);
 if(s.layout_id!=MUSIC_STEREO2_V1 || validateSession(s,s.mapping_bytes)!=StageBusValidationResult::Ok){error="Incompatible MUSIC Source StageBus";return false;}return true;
}
bool DspSessionDiscovery::write(const std::filesystem::path& path,const StageBusSessionHeader& s,const char* owner,std::string& error,const std::wstring& publishEndpoint){
 std::filesystem::create_directories(path.parent_path());QSaveFile f(QString::fromStdWString(path.wstring()));
 QJsonObject object{{"abi","0x00010000"},{"owner",owner},{"hardware_armed",false},{"session_header_base64",QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(&s),256).toBase64())}};
 if(!publishEndpoint.empty()){object.insert("stagepublish_protocol","StagePublishV2");object.insert("stagepublish_endpoint",QString::fromStdWString(publishEndpoint));object.insert("v1_external_takeover","Refused; subscribe to StagePublish V2");}
 auto data=QJsonDocument(object).toJson();if(!f.open(QIODevice::WriteOnly)||f.write(data)!=data.size()||!f.commit()){error="Discovery atomic write failed";return false;}return true;
}
}
