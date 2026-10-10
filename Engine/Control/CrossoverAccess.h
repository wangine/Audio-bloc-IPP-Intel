#pragma once
#include "Engine/AudioGraph.h"
#include "IPC/Json.h"
#include <entropy/crossover_abi.h>
#include <memory>
#include <vector>
namespace entropy::control {
class CrossoverAccess {
public:
 explicit CrossoverAccess(engine::AudioGraph&);
 ~CrossoverAccess();
 ipc::Value metadata() const;
 ipc::Value snapshot() const;
 void execute(const ipc::Value&,uint64_t session,uint64_t sequence);
 std::vector<unsigned char> exportBytes() const;
 void importBytes(const std::vector<unsigned char>&);
 void captureAfterBlock() noexcept;
 void captureBeforeBlock() noexcept;
 unsigned capturedSourceMask() const noexcept;
 bool settled() const noexcept;
 void resetQuiescent();
 bool preparedFinite() const noexcept;
 ipc::Value preparationParameters(const ipc::Value& change=ipc::Value{}) const;
 ipc::Value preparationCache() const;
private:
 struct Impl;std::unique_ptr<Impl> impl_;
};
}
