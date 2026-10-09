#include "SourceLinkPackage.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QSet>
#include <QStringList>
namespace entropy::stage {
bool verifySourceLinkPackage(const std::filesystem::path& root,std::string& error){
 const QString expectedContract="1428f9ce6d6ee5fb00ca320f9436dc831992edf04991d9c54596b9646e296ecb";
 QFile file(QString::fromStdWString((root/"package-manifest.json").wstring()));if(!file.open(QIODevice::ReadOnly)||file.size()>2*1024*1024){error="Package manifest missing/oversized";return false;}
 QJsonParseError parse{};auto document=QJsonDocument::fromJson(file.readAll(),&parse);auto manifest=document.object();
 if(parse.error!=QJsonParseError::NoError || manifest.value("stage").toString()!="SOURCE_STAGE1_LINK" || manifest.value("stagebus_contract_sha256").toString()!=expectedContract || !manifest.value("files").isArray()){error="Wrong Source link package contract";return false;}
 QSet<QString> paths;
 const QStringList optionalModules{"modules/EntropyMusicTransportEndpoint.dll","modules/EntropyAuxEndpoint.dll","modules/EntropyCinemaInput.dll","modules/EntropySampleRateConverter.dll","modules/EntropyTidalEndpoint.dll","modules/EntropyInputMatrix.dll"};
 for(auto item:manifest.value("files").toArray()){
  auto entry=item.toObject();auto name=entry.value("path").toString();auto hash=entry.value("sha256").toString();std::filesystem::path relative=name.toStdWString();
  if(name.isEmpty()||name.contains('\\')||relative.is_absolute()||relative.has_root_name()||name.split('/').contains("..")||hash.size()!=64 || paths.contains(name.toLower())){error="Invalid/duplicate package path";return false;}
  const bool optional=entry.value("optional").toBool();if(optional){error="Source link files are mandatory";return false;}paths.insert(name.toLower());
  auto target=root/relative;if(!std::filesystem::exists(target)){if(optional)continue;error="Missing immutable file: "+name.toStdString();return false;}
  std::error_code ec;auto canonical=std::filesystem::canonical(target,ec),base=std::filesystem::canonical(root,ec);auto rel=canonical.lexically_relative(base);if(ec||rel.empty()||*rel.begin()==".."){error="Package file escapes runtime";return false;}
  QFile payload(QString::fromStdWString(target.wstring()));if(!payload.open(QIODevice::ReadOnly)){error="Unreadable package file";return false;}
  QCryptographicHash digest(QCryptographicHash::Sha256);if(!digest.addData(&payload)||QString::fromLatin1(digest.result().toHex())!=hash){error="Altered immutable file: "+name.toStdString();return false;}
 }
 for(auto name:{"EntropySourceEngine.exe","sdk/StageBusContract.json","sdk/include/entropy/stage_bus_v1.h"})if(!paths.contains(QString::fromUtf8(name).toLower())){error="Incomplete Source link manifest";return false;}
 QFile contract(QString::fromStdWString((root/"sdk"/"StageBusContract.json").wstring()));if(!contract.open(QIODevice::ReadOnly)||QString::fromLatin1(QCryptographicHash::hash(contract.readAll(),QCryptographicHash::Sha256).toHex())!=expectedContract){error="SDK contract is not frozen Stage1";return false;}
 if(std::filesystem::exists(root/"modules"))for(auto& entry:std::filesystem::directory_iterator(root/"modules")){auto name=QString::fromStdWString(entry.path().filename().wstring());if(name.endsWith(".dll",Qt::CaseInsensitive)&&!optionalModules.contains("modules/"+name)){error="Unexpected native audio module: "+name.toStdString();return false;}}
 return true;
}
}
