#include "DspPackage.h"
#include "DspNativeRegistry.h"
#include "MusicStageBusHashes.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QSet>
#include <QStringList>
namespace entropy::stage {
bool verifyDspPackage(const std::filesystem::path& root,std::string& error,bool requireCenter,bool requireAligner,bool requireEq){
 const QString expectedContract="1428f9ce6d6ee5fb00ca320f9436dc831992edf04991d9c54596b9646e296ecb";
 QFile file(QString::fromStdWString((root/"package-manifest.json").wstring()));if(!file.open(QIODevice::ReadOnly)||file.size()>2*1024*1024){error="Package manifest missing/oversized";return false;}
 QJsonParseError parse{};auto document=QJsonDocument::fromJson(file.readAll(),&parse);auto manifest=document.object();
 if(parse.error!=QJsonParseError::NoError || manifest.value("stage").toString()!="DSP_STAGE2" || manifest.value("stagebus_contract_sha256").toString()!=expectedContract || !manifest.value("files").isArray()){error="Wrong Stage2 package contract";return false;}
 if(manifest.value("music_stagebus_contract_sha256").toString()!=QString::fromLatin1(musicContractHex) ||
    manifest.value("input_channels").toInt()!=2 || manifest.value("output_channels").toInt()!=3){error="Stage2 MUSIC requires pinned 2 IN / 3 OUT contract";return false;}
 QSet<QString> paths;
 const bool activeCenter=requireCenter || manifest.value("dsp_mode").toString()=="CENTER_V2_C_ONLY";
 const bool activeAligner=requireAligner||manifest.value("dsp_mode").toString()=="CENTER_V2_LRC_STATIC";
 const bool activeEq=requireEq||manifest.value("parametric_eq_available").toBool();
 bool centerVerified=false,alignerVerified=false,eqVerified=false,recoveryDeclared=false;
 const QStringList optionalModules{"modules/EntropyCenterExtractorFP64.dll","modules/EntropyCenterExtractorV2FP64.dll","modules/EntropyLoudness.dll","modules/EntropyParametricEQ.dll","modules/EntropyLrcAlignerFP64.dll","modules/EntropyDynamicRecovery.dll"};
 for(auto item:manifest.value("files").toArray()){
  auto entry=item.toObject();auto name=entry.value("path").toString();auto hash=entry.value("sha256").toString();std::filesystem::path relative=name.toStdWString();
  if(name.isEmpty()||name.contains('\\')||relative.is_absolute()||relative.has_root_name()||name.split('/').contains("..")||hash.size()!=64 || paths.contains(name.toLower())){error="Invalid/duplicate package path";return false;}
  const bool optional=entry.value("optional").toBool();if(optional&&!optionalModules.contains(name)){error="Only native DSP modules may be optional";return false;}paths.insert(name.toLower());
  if(activeCenter && name=="modules/EntropyCenterExtractorV2FP64.dll"){
   if(optional || hash!="e96bf561bfa204f7aada02e2245e264eb8db2e3f1e9a06ecc07caf43e0ba32f0"){error="Active Center requires original mandatory V2 DLL";return false;}centerVerified=true;
  }
  if(activeAligner && name=="modules/EntropyLrcAlignerFP64.dll"){
   if(optional || hash!=nativeModule(NativeModuleId::LrcAligner)->sha256){error="LRC requires verified mandatory standalone DLL";return false;}alignerVerified=true;
  }
  if(activeEq&&name=="modules/EntropyParametricEQ.dll"){
   if(optional||hash!=nativeModule(NativeModuleId::ParametricEq)->sha256){error="EQ requires original mandatory FP64 DLL";return false;}eqVerified=true;
  }
  if(name=="modules/EntropyDynamicRecovery.dll"){
   if(hash!=nativeModule(NativeModuleId::DynamicRecovery)->sha256){error="Dynamic Recovery manifest SHA mismatch";return false;}
   recoveryDeclared=true;
  }
  auto target=root/relative;if(!std::filesystem::exists(target)){if(optional)continue;error="Missing immutable file: "+name.toStdString();return false;}
  std::error_code ec;auto canonical=std::filesystem::canonical(target,ec),base=std::filesystem::canonical(root,ec);auto rel=canonical.lexically_relative(base);if(ec||rel.empty()||*rel.begin()==".."){error="Package file escapes runtime";return false;}
  QFile payload(QString::fromStdWString(target.wstring()));if(!payload.open(QIODevice::ReadOnly)){error="Unreadable package file";return false;}
  QCryptographicHash digest(QCryptographicHash::Sha256);if(!digest.addData(&payload)||QString::fromLatin1(digest.result().toHex())!=hash){error="Altered immutable file: "+name.toStdString();return false;}
 }
 if(activeAligner&&!alignerVerified){error="LRC absent from manifest";return false;}
 if(activeCenter&&!centerVerified){error="Active Center V2 absent from manifest";return false;}
 if(activeEq&&!eqVerified){error="EQ absent from manifest";return false;}
 if(manifest.value("dynamic_recovery_available").toBool()&&!recoveryDeclared){error="Dynamic Recovery absent from manifest";return false;}
 for(auto name:{"EntropyDspEngine.exe","sdk/StageBusContract.json","sdk/MusicStageBusContract.json","sdk/include/entropy/stage_bus_v1.h"})if(!paths.contains(QString::fromUtf8(name).toLower())){error="Incomplete Stage2 manifest";return false;}
 QFile contract(QString::fromStdWString((root/"sdk"/"StageBusContract.json").wstring()));if(!contract.open(QIODevice::ReadOnly)||QString::fromLatin1(QCryptographicHash::hash(contract.readAll(),QCryptographicHash::Sha256).toHex())!=expectedContract){error="SDK contract is not frozen Stage1";return false;}
 QFile musicContract(QString::fromStdWString((root/"sdk"/"MusicStageBusContract.json").wstring()));
 if(!musicContract.open(QIODevice::ReadOnly)||QString::fromLatin1(QCryptographicHash::hash(musicContract.readAll(),QCryptographicHash::Sha256).toHex())!=QString::fromLatin1(musicContractHex)){error="SDK MUSIC contract differs from the binary";return false;}
 if(std::filesystem::exists(root/"modules"))for(auto& entry:std::filesystem::directory_iterator(root/"modules")){auto name=QString::fromStdWString(entry.path().filename().wstring());if(name.endsWith(".dll",Qt::CaseInsensitive)&&(!optionalModules.contains("modules/"+name)||!paths.contains(("modules/"+name).toLower()))){error="Unexpected native audio module: "+name.toStdString();return false;}}
 return true;
}
}
