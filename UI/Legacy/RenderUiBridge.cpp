#include "RenderUiBridge.h"
#include "Client/EngineFacade.h"
#include <cmath>
#include <algorithm>
#include <cwchar>
namespace {std::atomic<entropy::stage::RenderUiBridge*> active{nullptr};}
namespace entropy::stage {
RenderUiBridge::RenderUiBridge(){active.store(this,std::memory_order_release);}
RenderUiBridge::~RenderUiBridge(){auto* expected=this;active.compare_exchange_strong(expected,nullptr);}
RenderUiBridge* RenderUiBridge::current() noexcept {return active.load(std::memory_order_acquire);}
bool RenderUiBridge::enqueue(entropy::host::EngineControlSnapshot& c,uint64_t& sequence) noexcept {
 auto* client=entropy::ui::EngineFacade::current();return client&&client->submit(c,sequence);
}
void RenderUiBridge::publish(const entropy::host::EngineTelemetrySnapshot& snapshot) noexcept {
 static_assert(std::atomic<uint64_t>::is_always_lock_free);revision_.fetch_add(1,std::memory_order_acq_rel);
 for(size_t i=0;i<telemetry_.size();i++){uint64_t word=0;const auto offset=i*8;const auto size=std::min(size_t(8),sizeof(snapshot)-offset);std::memcpy(&word,reinterpret_cast<const char*>(&snapshot)+offset,size);telemetry_[i].store(word,std::memory_order_relaxed);}revision_.fetch_add(1,std::memory_order_release);
}
bool RenderUiBridge::read(entropy::host::EngineTelemetrySnapshot& snapshot) const noexcept {
 for(unsigned attempt=0;attempt<4;attempt++){auto begin=revision_.load(std::memory_order_acquire);if(!begin || begin&1u)continue;for(size_t i=0;i<telemetry_.size();i++){auto word=telemetry_[i].load(std::memory_order_relaxed);const auto offset=i*8;std::memcpy(reinterpret_cast<char*>(&snapshot)+offset,&word,std::min(size_t(8),sizeof(snapshot)-offset));}std::atomic_thread_fence(std::memory_order_acquire);if(begin==revision_.load(std::memory_order_acquire))return true;}return false;
}
}
namespace entropy::host {
namespace {
bool send(EngineControlSnapshot& c,uint64_t& seq) noexcept {auto* bridge=entropy::stage::RenderUiBridge::current();return bridge && bridge->enqueue(c,seq);}
bool head(EngineHeadroomAction action,uint32_t index,uint32_t secondary,double value,uint64_t& seq) noexcept {EngineControlSnapshot c{};c.headroomSequence=1;c.headroomAction=action;c.headroomIndex=index;c.headroomSecondaryIndex=secondary;c.headroomValue=value;return send(c,seq);}
bool bulk(uint32_t action,bool cinema,bool right,double value) noexcept {EngineControlSnapshot c{};c.crossoverBulkSequence=1;c.crossoverBulkAction=action;c.crossoverBulkProgram=cinema;c.crossoverBulkSide=right;c.crossoverBulkValue=value;uint64_t seq;return send(c,seq);}
bool hardware(uint32_t action,const EngineDacDomainControl* domains,uint32_t count,uint64_t& seq) noexcept {if(count>engineMaximumDacDomains || (count && !domains))return false;EngineControlSnapshot c{};c.hardwareOutputSequence=1;c.hardwareOutputAction=action;for(unsigned n=0;n<count;n++)c.hardwareOutputDomains[n]=domains[n];return send(c,seq);}
}
bool readEngineTelemetry(EngineTelemetrySnapshot& s) noexcept {auto* bridge=entropy::stage::RenderUiBridge::current();if(!bridge || !bridge->read(s))return false;entropy::stage::presentSelectedCrossoverProgram(s);return true;}
bool writeEngineParameterIntent(const char* module,const char* id,double value,const wchar_t*) noexcept {if(!module || !id || std::strcmp(module,"audio.entropy.crossover") || std::strlen(id)>=48 || !std::isfinite(value))return false;EngineControlSnapshot c{};c.parameterSequence=1;c.parameterAction=EngineParameterSet;std::strcpy(c.parameterIdentifier,id);std::strcpy(c.moduleIdentifier,module);c.parameterValue=value;uint64_t seq;return send(c,seq);}
bool writeEngineParameterGesture(const char* module,const char* id,bool begin,const wchar_t*) noexcept {if(!module || !id || std::strcmp(module,"audio.entropy.crossover") || std::strlen(id)>=48)return false;EngineControlSnapshot c{};c.parameterSequence=1;c.parameterAction=begin?EngineParameterBeginGesture:EngineParameterEndGesture;std::strcpy(c.parameterIdentifier,id);std::strcpy(c.moduleIdentifier,module);uint64_t seq;return send(c,seq);}
bool writeEngineCrossoverMonitorChannel(int channel) noexcept {if(channel< -1 || channel>=16)return false;EngineControlSnapshot c{};c.crossoverMonitorSequence=1;c.crossoverMonitorChannel=channel;uint64_t seq;return send(c,seq);}
bool writeEngineCrossoverApplySideDelay(bool cinema,bool right,double value) noexcept {return std::isfinite(value)&&bulk(EngineCrossoverApplySideDelay,cinema,right,value);}
bool writeEngineCrossoverApplyLeftToRight(bool cinema) noexcept {return bulk(EngineCrossoverApplyLeftToRight,cinema,false,0);}
bool writeEngineCrossoverResetProtectionTrip(uint32_t channel) noexcept {return channel<16 && bulk(EngineCrossoverResetProtectionTrip,false,false,double(channel));}
bool writeEngineCrossoverResetAllProtectionTrips() noexcept {return bulk(EngineCrossoverResetAllProtectionTrips,false,false,0);}
bool writeEngineOutputMatrixRoute(uint32_t source,uint32_t destination,bool enabled,uint64_t& seq) noexcept {if(source>=16 || destination>=16)return false;EngineControlSnapshot c{};c.outputMatrixSequence=1;c.outputMatrixAction=EngineOutputMatrixSetRoute;c.outputMatrixSource=source;c.outputMatrixDestination=destination;c.outputMatrixEnabled=enabled;return send(c,seq);}
bool writeEngineOutputMatrixClearAll(uint64_t& seq) noexcept {EngineControlSnapshot c{};c.outputMatrixSequence=1;c.outputMatrixAction=EngineOutputMatrixClearAll;return send(c,seq);}
bool writeEngineHeadroomTransitionMs(double v,uint64_t& seq) noexcept {return head(EngineHeadroomSetTransitionMs,0,0,v,seq);}
bool writeEngineHeadroomAutoAttackMs(double v,uint64_t& seq) noexcept {return head(EngineHeadroomSetAutoAttackMs,0,0,v,seq);}
bool writeEngineHeadroomAutoReleaseMs(double v,uint64_t& seq) noexcept {return head(EngineHeadroomSetAutoReleaseMs,0,0,v,seq);}
bool writeEngineHeadroomGroupCount(uint32_t v,uint64_t& seq) noexcept {return head(EngineHeadroomSetGroupCount,v,0,0,seq);}
bool writeEngineHeadroomChannelGroup(uint32_t c,uint32_t g,uint64_t& seq) noexcept {return head(EngineHeadroomSetChannelGroup,c,g,0,seq);}
bool writeEngineHeadroomPreventiveReserve(uint32_t g,double v,uint64_t& seq) noexcept {return head(EngineHeadroomSetPreventiveReserve,g,0,v,seq);}
bool writeEngineHeadroomClearFault(uint64_t& seq) noexcept {return head(EngineHeadroomClearFault,0,0,0,seq);}
bool writeEngineHeadroomOperatingMode(uint32_t v,uint64_t& seq) noexcept {return head(EngineHeadroomSetOperatingMode,v,0,-6,seq);}
bool writeEngineHeadroomOperatingPolicy(uint32_t v,double db,uint64_t& seq) noexcept {return head(EngineHeadroomSetOperatingMode,v,0,db,seq);}
bool writeEngineHardwareOutputArm(const EngineDacDomainControl* d,uint32_t count,uint64_t& seq) noexcept {return hardware(EngineHardwareOutputArm,d,count,seq);}
bool writeEngineHardwareOutputConfiguration(const EngineDacDomainControl* d,uint32_t count,uint64_t& seq) noexcept {return hardware(EngineHardwareOutputConfigure,d,count,seq);}
bool writeEngineHardwareOutputDisarm(uint64_t& seq) noexcept {return hardware(EngineHardwareOutputDisarm,nullptr,0,seq);}
bool writeEngineProjectRequest(EngineProjectAction action,const wchar_t* path,uint64_t& seq) noexcept {if(!path || std::wcslen(path)>=512 || (action!=EngineProjectSave && action!=EngineProjectLoad))return false;EngineControlSnapshot c{};c.projectSequence=1;c.projectAction=action;std::wcsncpy(c.projectPath,path,511);return send(c,seq);}
}
