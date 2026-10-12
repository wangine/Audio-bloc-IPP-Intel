#include "MusicPublisher.h"
#include "StageBusValidation.h"
#include "StagePublishV2.h"
#include <atomic>
#include <cstring>
#include <objbase.h>
#include <windows.h>
namespace entropy::music {
struct MusicPublisher::Impl {
    entropy::publish::Publisher publisher;
    StageBusSessionHeader session{};
    std::atomic<uint64_t> sequence{0};
};
MusicPublisher::MusicPublisher() : impl_(std::make_unique<Impl>()) {}
MusicPublisher::~MusicPublisher() = default;
bool MusicPublisher::start(const std::wstring &pipe, std::string &error) {
    GUID uuid{};
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    if (FAILED(CoCreateGuid(&uuid))) {
        error = "UUID creation failed";
        return false;
    }
    auto &p = *impl_;
    p.session = entropy::stage::makeSession(MUSIC_STEREO2_V1, reinterpret_cast<uint8_t *>(&uuid), f.QuadPart);
    p.session.producer_pid = GetCurrentProcessId();
    if (!p.publisher.start(pipe, error))
        return false;
    p.publisher.setSession(p.session);
    return true;
}
void MusicPublisher::service() noexcept { impl_->publisher.service(); }
void MusicPublisher::publish(const ProgramBlock &program) noexcept {
    auto &p = *impl_;
    if (!p.session.magic)
        return;
    StageBusBlockHeader b{};
    memcpy(b.session_id, p.session.session_id, 16);
    b.stream_epoch = 1;
    b.block_sequence = p.sequence.load(std::memory_order_relaxed) + 1;
    b.first_frame = (b.block_sequence - 1) * 1024;
    b.control_revision = program.controlRevision;
    b.music_session_revision = program.musicRevision;
    b.frames = 1024;
    b.validity_mask = program.validity & 3;
    b.discontinuity_mask = program.discontinuity & 3;
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    b.qpc_timestamp = q.QuadPart;
    for (unsigned i = 0; i < 2; i++) {
        b.source_generation[i] = program.sources[i].generation;
        b.source_rate[i] = program.sources[i].rate;
        b.source_state[i] = (b.validity_mask & (1u << i)) ? 2 : 0;
    }
    const double *planes[] = {program.audio.left.data(), program.audio.right.data()};
    p.publisher.publish(b, planes);
    p.sequence.store(b.block_sequence, std::memory_order_release);
}
void MusicPublisher::stop() { impl_->publisher.stop(); }
uint64_t MusicPublisher::sequence() const noexcept { return impl_->sequence.load(); }
uint64_t MusicPublisher::dropped() const noexcept { return impl_->publisher.dropped(); }
const StageBusSessionHeader &MusicPublisher::session() const noexcept { return impl_->session; }
} // namespace entropy::music
