#pragma once
#include "DspStageRuntime.h"
#include "DspAutoLink.h"

#include <atomic>
#include <filesystem>
#include <thread>
namespace entropy::stage {
enum class DspStateCode:uint64_t {Stopped,WaitingDownstream,Running,Fault,WaitingSource};
struct DspUiSnapshot {DspStateCode state=DspStateCode::Stopped;DspDiagnostics diag{};StageBusSessionHeader outputSession{};char error[192]{};};
static_assert(sizeof(DspUiSnapshot)%8==0);
class DspController {
public:
 ~DspController(){stop();}
 bool start(const std::filesystem::path& runtimeRoot,const std::filesystem::path& sourceDiscovery,std::string& error);
 bool startAutomatic(const std::filesystem::path& runtimeRoot,std::string& error,std::wstring endpoint=entropy::publish::sourceEndpointName());
 void stop() noexcept;
 DspUiSnapshot snapshot() const noexcept;
 bool configureAligner(const std::filesystem::path&,std::string&);
 bool configureEq(const std::filesystem::path&,const EqSettings&,std::string&);
 bool requestEq(const EqSettings&,uint64_t& revision) noexcept;
 bool requestedEqSettings(EqSettings& settings) const noexcept{uint64_t revision=0;EqSettings value{};if(!readEqRequest(value,revision))return false;settings=value;return true;}
 bool eqSchema(EqSchema& schema) const noexcept {if(!eqSchemaReady_.load(std::memory_order_acquire))return false;schema=eqSchema_;return true;}
 bool requestAlignment(bool) noexcept;
 bool configureCenter(const std::filesystem::path&,const CenterSettings&,std::string&);
 bool requestedCenterSettings(CenterSettings& settings) const noexcept {uint64_t revision=0;return readCenterRequest(settings,revision);}
 bool requestCenter(const CenterSettings&) noexcept; // single UI/control publisher, never native calls
 bool startCapture(const std::filesystem::path&,uint64_t blocks,std::string& error);
private:
 friend struct DiscoveryRaceProbe;
 void (*attachmentProbe_)(void*)=nullptr;
 void* probeContext_=nullptr;
 bool prepareAlignerWorker(std::string&);
 bool prepareEqWorker(std::string&);
 bool adoptEqRequest() noexcept;
 bool readEqRequest(EqSettings&,uint64_t&) const noexcept;
 std::filesystem::path eqModuleRoot_;
 bool eqConfigured_=false;
 std::atomic<uint64_t> eqRequestRevision_{0};
 std::array<std::atomic<uint64_t>,sizeof(EqSettings)/8> eqRequestValues_{};
 uint64_t eqSubmittedRevision_=0;
 // Immutable for a worker lifetime; published once before readiness release.
 EqSchema eqSchema_{};std::atomic<bool> eqSchemaReady_{false};
 bool adoptAlignmentRequest() noexcept;
 bool readCenterRequest(CenterSettings&,uint64_t&) const noexcept;
 bool prepareCenterWorker(std::string&);
 bool adoptCenterRequest() noexcept;

 void automaticWorker() noexcept;
 void publish(DspStateCode,const char* error="") noexcept;
 std::filesystem::path alignerModuleRoot_;
 bool alignerConfigured_=false;
 std::atomic<uint64_t> alignmentRequestRevision_{0};
 std::array<std::atomic<uint64_t>,3> alignmentRequestValues_{};
 uint64_t alignmentAppliedRevision_=0;
 std::filesystem::path centerModuleRoot_;
 bool centerConfigured_=false;
 std::atomic<uint64_t> centerRequestRevision_{0};
 std::array<std::atomic<uint64_t>,16> centerRequestValues_{};
 uint64_t centerAppliedRevision_=0;
 DspStageRuntime runtime_;
 DspRecorder recorder_;
 std::unique_ptr<DspAutoLink> autoLink_;

 std::unique_ptr<entropy::publish::Publisher> publisher_;
 std::wstring publishEndpoint_;

 std::filesystem::path runtimeRoot_;
 HANDLE automaticOwner_=nullptr;
 std::thread thread_;
 std::atomic<bool> stopping_{false};
 std::atomic<uint64_t> snapshotRevision_{0};
 std::array<std::atomic<uint64_t>,sizeof(DspUiSnapshot)/8> snapshotWords_{};
};
}
