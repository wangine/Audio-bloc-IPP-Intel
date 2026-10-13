#pragma once
#include "StageBusWin32.h"
#include "StageBusDemand.h"
#include "TransparentDspExecutor.h"
#include "DspRecorder.h"
#include "DspSignalTelemetry.h"
#include "DspCenterModule.h"
#include "DspAlignment.h"
#include "DspLrcModule.h"
#include "DspEqModule.h"
#include "StagePublishV2.h"
#include <array>
#include <optional>
namespace entropy::stage {
enum class DspStepResult {Idle,Progress,Published,Fault};
struct DspDiagnostics {
 uint64_t blocksIn=0,blocksOut=0,framesIn=0,framesOut=0,invalid=0,missing=0,duplicates=0,faults=0,full=0,deadlines=0,captureDrops=0;
 uint64_t executorQpc=0,traversalQpc=0,queueQpc=0,maxExecutorQpc=0,maxTraversalQpc=0,maxQueueQpc=0;
 std::array<std::array<uint64_t,3>,3> latencyPercentilesUs{};
 StageBusBlockHeader lastHeader{};
 DspSignalSnapshot signal{};
 CenterSnapshot center{};
 EqSnapshot eq{};
 AlignmentSnapshot alignment{};
 uint64_t monitorScheduleRebases=0,publishDrops=0;
};
class DspStageRuntime {
public:
 ~DspStageRuntime(){stop();}
 bool attach(const StageBusSessionHeader&,std::string& error);
 bool attachPassive(const std::wstring& endpoint,std::string& error);
 bool prepareEq(const std::filesystem::path&,const EqSettings&,std::string&,uint64_t revision=1);
 bool applyEq(const EqSettings&,uint64_t) noexcept;
 bool eqCommandPending() const noexcept{return eq_.pending();}
 bool pollEqTelemetry() noexcept {if(!eq_.loaded())return true;if(!eq_.readControlSnapshot())return false;diag_.eq=eq_.snapshot();return true;}
 const EqSchema& eqSchema() const noexcept{return eq_.schema();}
 void closeEq() noexcept{eq_.close();diag_.eq=eq_.snapshot();}
 bool prepareCenter(const std::filesystem::path&,const CenterSettings&,std::string&);
 bool applyCenter(const CenterSettings&,uint64_t revision) noexcept;
 void closeCenter() noexcept {center_.close();diag_.center=center_.snapshot();}
 bool prepareAligner(const std::filesystem::path&,std::string&);
 void closeAligner() noexcept {aligner_.close();diag_.alignment.loaded=0;diag_.alignment.valid=0;}
 void acknowledgeAlignment(uint64_t serial,bool rejected) noexcept {diag_.alignment.commandSerial=serial;diag_.alignment.rejected=rejected;}
 AlignmentApplyResult applyAlignment(bool,uint64_t,uint64_t) noexcept;
 bool centerLoaded() const noexcept {return center_.loaded();}
 DspStepResult step(uint64_t nowQpc) noexcept;
 void stop() noexcept;
 void heartbeat(uint64_t now) noexcept;
 bool peersAlive(uint64_t now) noexcept; // control worker only: OpenProcess/Wait
 const StageBusSessionHeader& outputSession() const noexcept{return outputSession_;}
 const DspDiagnostics& diagnostics() const noexcept{return diag_;}
 void summarizeLatency(DspDiagnostics&) const noexcept; // control/diagnostics only
 bool pending() const noexcept{return ready_;}
 bool downstreamReady() const noexcept{return downstreamReady_;}
 bool faulted() const noexcept{return faulted_;}
 void setRecorder(DspRecorder* recorder) noexcept{if(!attached_)recorder_=recorder;}
 void setPublisher(entropy::publish::Publisher* publisher) noexcept{publisher_=publisher;}
private:
 void refreshAlignment() noexcept;
 void recordLatency(unsigned,uint64_t) noexcept;
 DspStepResult fail(bool invalid=false) noexcept;
 bool resetHistory() noexcept;
 entropy::publish::Subscriber input_;
 StageBusSessionHeader inputSession_{},outputSession_{};
 TransparentDspExecutor executor_;
 DspCenterModule center_;
 DspEqModule eq_;
 DspLrcModule aligner_;
 std::array<std::array<double,1024>,3> alignedPlanes_{};
 std::array<std::array<double,1024>,2> inputPlanes_{};
 std::array<std::array<double,1024>,3> outputPlanes_{};
 std::array<double*,2> inputPointers_{};
 std::array<const double*,2> inputConst_{};
 std::array<double*,3> outputPointers_{};
 std::array<const double*,3> outputConst_{};
 StageBusBlockHeader pendingHeader_{},lastHeader_{};
 DspDiagnostics diag_{};
 std::array<std::array<uint64_t,2049>,3> latencyBins_{};
 std::array<uint64_t,3> latencyCounts_{};
 uint64_t received_=0;
 bool attached_=false,faulted_=false,ready_=false,downstreamReady_=false;
 bool haveInput_=false;
 DspRecorder* recorder_=nullptr;
 entropy::publish::Publisher* publisher_=nullptr;
};
}
