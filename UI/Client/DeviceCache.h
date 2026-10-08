#pragma once
#include "EngineFacade.h"
#include <entropy/device_manager_abi.h>
#include <QJsonArray>
#include <cstring>
namespace entropy::ui {
template<size_t N>inline void copyDeviceText(char(&dst)[N],const QJsonValue& v){auto b=v.toString().toUtf8();auto n=std::min(size_t(b.size()),N-1);std::memcpy(dst,b.data(),n);dst[n]=0;}
inline EntropyDeviceInfo deviceInfo(const QJsonObject& d){EntropyDeviceInfo info{};info.struct_size=sizeof(info);info.active=d["active"].toBool();info.is_default=d["default"].toBool();info.maximum_channels=d["maximumChannels"].toInt();copyDeviceText(info.endpoint_id_utf8,d["endpoint"]);copyDeviceText(info.display_name_utf8,d["name"]);copyDeviceText(info.composite_fingerprint,d["fingerprint"]);
 auto formats=d["formats"].toArray(),evidence=d["evidence"].toArray();info.format_count=uint32_t(std::min(qsizetype(32),formats.size()));info.evidence_count=uint32_t(std::min(qsizetype(20),evidence.size()));for(unsigned i=0;i<info.format_count;++i){auto f=formats[int(i)].toObject();info.formats[i]={sizeof(EntropyDeviceExactFormat),uint32_t(f["rate"].toInt()),uint32_t(f["channels"].toInt()),uint32_t(f["containerBits"].toInt()),uint32_t(f["validBits"].toInt()),uint32_t(f["encoding"].toInt())};}for(unsigned i=0;i<info.evidence_count;++i){auto e=evidence[int(i)].toObject();auto& target=info.evidence[i];target.struct_size=sizeof(target);target.kind=e["kind"].toInt();target.stability=e["stability"].toInt();target.flags=e["flags"].toInt();copyDeviceText(target.value_utf8,e["value"]);}return info;
}
inline EntropyDeviceBinding deviceBinding(const QJsonObject& b){EntropyDeviceBinding target{};target.struct_size=sizeof(target);target.schema_version=ENTROPY_DEVICE_BINDING_SCHEMA_VERSION;target.approval_revision=b["approvalRevision"].toString().toULongLong();copyDeviceText(target.logical_device_id,b["logicalId"]);copyDeviceText(target.approved_endpoint_id_utf8,b["endpoint"]);copyDeviceText(target.approved_display_name_utf8,b["name"]);copyDeviceText(target.composite_fingerprint,b["fingerprint"]);return target;}
inline void requestDevices(QJsonObject request){if(auto* facade=EngineFacade::current())facade->request(std::move(request),[](const QJsonObject& r){if(auto* current=EngineFacade::current())current->updateDevices(r);});}
}
