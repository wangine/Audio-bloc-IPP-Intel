#include "DspEqState.h"
#include <QJsonArray>
#include <cmath>
namespace entropy::stage {
QJsonObject eqToJson(const EqSettings& settings){
 QJsonArray bands;
 for(const auto& b:settings.bands)bands.append(QJsonObject{{"enabled",bool(b.enabled)},{"filter_type",int(b.filter_type)},{"detector_mode",int(b.detector_mode)},{"detector_source",int(b.detector_source)},{"dynamics_mode",int(b.dynamics_mode)},{"frequency_hz",b.frequency_hz},{"q",b.q},{"static_gain_db",b.static_gain_db},{"threshold_db",b.threshold_db},{"ratio",b.ratio},{"knee_db",b.knee_db},{"attack_ms",b.attack_ms},{"release_ms",b.release_ms},{"max_cut_db",b.max_cut_db},{"max_boost_db",b.max_boost_db}});
 return {{"mode",int(settings.mode)},{"bands",bands}};
}
bool eqFromJson(const QJsonObject& o,EqSettings& output){
 EqSettings s=defaultEqSettings();const auto integer=[](const QJsonValue& v,uint32_t& out){if(!v.isDouble())return false;double d=v.toDouble();if(!std::isfinite(d)||d<0||d>3||d!=std::floor(d))return false;out=static_cast<uint32_t>(d);return true;};
 if(o.size()!=2||!integer(o.value("mode"),s.mode)||!o.value("bands").isArray())return false;
 auto bands=o.value("bands").toArray();if(bands.size()!=8)return false;
 for(unsigned i=0;i<8;i++){
  if(!bands[i].isObject())return false;auto object=bands[i].toObject();auto& b=s.bands[i];if(object.size()!=15||!object.value("enabled").isBool())return false;b.enabled=object.value("enabled").toBool();
  if(!integer(object.value("filter_type"),b.filter_type)||!integer(object.value("detector_mode"),b.detector_mode)||!integer(object.value("detector_source"),b.detector_source)||!integer(object.value("dynamics_mode"),b.dynamics_mode))return false;
  const char* keys[]={"frequency_hz","q","static_gain_db","threshold_db","ratio","knee_db","attack_ms","release_ms","max_cut_db","max_boost_db"};double* values[]={&b.frequency_hz,&b.q,&b.static_gain_db,&b.threshold_db,&b.ratio,&b.knee_db,&b.attack_ms,&b.release_ms,&b.max_cut_db,&b.max_boost_db};
  for(unsigned k=0;k<10;k++){auto v=object.value(keys[k]);if(!v.isDouble())return false;*values[k]=v.toDouble();}
 }
 if(!validEqSettings(s))return false;output=s;return true;
}
}
