#pragma once
#include "AudioBlockReport.h"
#include <entropy/input_matrix_abi.h>
#include <entropy/headroom_abi.h>
#include <entropy/center_abi.h>
#include <memory>
namespace entropy::host {
class GraphRuntime; class ModuleInstance; class R05DiagnosticCapture; class R05BlockRecorder;
// Bound/stored on the control owner before processing. Rebind only while stopped.
class AuthoritativeAudioExecutor final {
public:
 AuthoritativeAudioExecutor();
 ~AuthoritativeAudioExecutor();
 bool prepare(GraphRuntime&);
 void setDiagnosticCapture(R05DiagnosticCapture* capture) noexcept { diagnosticCapture_=capture; }
 AudioBlockReport process(const AudioBlockReport& tidal,const AudioBlockReport& aux,const AudioBlockReport& cinema,
   bool muteTidal,bool muteAux,bool muteCinema,bool musicProgramActive,bool analyzeMusicFinal,
   std::uint32_t forcedFrames,const EntropyCrossoverProgramBlockApiV1* programApi=nullptr,
   const EntropyCrossoverProgramInputs* programInputs=nullptr) noexcept;
private:
 R05DiagnosticCapture* diagnosticCapture_=nullptr;
 std::unique_ptr<R05BlockRecorder> diagnosticRecorder_;
 std::uint64_t diagnosticSequence_=0,diagnosticOffset_=0;
 struct Scratch;
 std::unique_ptr<Scratch> scratch_;
 GraphRuntime* runtime_=nullptr;
 ModuleInstance *matrix_=nullptr,*center_=nullptr,*crossover_=nullptr,*outputMatrix_=nullptr,*headroom_=nullptr;
 EntropyInputMatrixApiV1 matrixApi_{};
 EntropyHeadroomAnalyzerApiV4 headroomAnalyzer_{};
 EntropyCenterTelemetryApiV1 centerTelemetryApi_{};
 bool ready_=false;
};
}
