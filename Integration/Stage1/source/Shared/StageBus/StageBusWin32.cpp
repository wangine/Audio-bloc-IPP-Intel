#include "StageBusWin32.h"
#include <cstring>
#include <iomanip>
#include <windows.h>
#include <sddl.h>
#include <sstream>
#include <vector>

namespace entropy::stage
{
namespace
{
struct alignas(64) Counter
{
    volatile LONG64 value;
    char pad[56];
};
struct Shared
{
    StageBusSessionHeader header;
    Counter write, read, producerHeartbeat, consumerHeartbeat, requestWrite, requestRead;
    StageBusFrameRequest request;
    char pad[8];
    Counter responseWrite, responseRead;
    StageBusBlockHeader response;
    Counter ready, fault;
    Counter sourceControlWrite, sourceControlRead;
    StageBusSourceControl sourceControl;
};
static_assert(offsetof(Shared, write) == 256 && offsetof(Shared, read) == 320);
static_assert(offsetof(Shared, request) == 640 && offsetof(Shared, ready) == 1024 &&
              sizeof(Shared) == 1344);
LONG64 load(const Counter &c) noexcept
{
    return InterlockedCompareExchange64(const_cast<volatile LONG64 *>(&c.value), 0, 0);
}
void store(Counter &c, uint64_t value) noexcept
{
    InterlockedExchange64(&c.value, static_cast<LONG64>(value));
}
std::wstring name(const StageBusSessionHeader &s, const wchar_t *kind)
{
    std::wostringstream out;
    out << L"Local\\Entropy.StageBus.V1.";
    for (auto b : s.session_id)
        out << std::hex << std::setw(2) << std::setfill(L'0') << int(b);
    out << L"." << s.layout_id << L"." << kind;
    return out.str();
}
struct Security
{
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    SECURITY_ATTRIBUTES attributes{};
    ~Security()
    {
        if (descriptor)
            LocalFree(descriptor);
    }
    bool init()
    {
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
            return false;
        DWORD bytes = 0;
        GetTokenInformation(token, TokenUser, nullptr, 0, &bytes);
        std::vector<unsigned char> data(bytes);
        bool ok = GetTokenInformation(token, TokenUser, data.data(), bytes, &bytes);
        CloseHandle(token);
        if (!ok)
            return false;
        wchar_t *sid = nullptr;
        if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(data.data())->User.Sid, &sid))
            return false;
        std::wstring acl = L"D:P(A;;GA;;;" + std::wstring(sid) + L")";
        LocalFree(sid);
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(), SDDL_REVISION_1,
                                                                  &descriptor, nullptr))
            return false;
        attributes = {sizeof(attributes), descriptor, FALSE};
        return true;
    }
};
} // namespace
StageBusWin32::~StageBusWin32() { close(); }
void StageBusWin32::close() noexcept
{
    if (view_)
        UnmapViewOfFile(view_);
    if (peer_)
        CloseHandle(peer_);
    if (event_)
        CloseHandle(event_);
    if (mapping_)
        CloseHandle(mapping_);
    view_ = peer_ = event_ = mapping_ = nullptr;
    lastSequence_ = lastEpoch_ = 0;
    producer_ = false;
}
bool StageBusWin32::create(const StageBusSessionHeader &s, std::string &error)
{
    close();
    if (validateSession(s, s.mapping_bytes) != StageBusValidationResult::Ok)
    {
        error = "Invalid session contract";
        return false;
    }
    Security security;
    if (!security.init())
    {
        error = "Cannot restrict IPC ACL";
        return false;
    }
    mapping_ = CreateFileMappingW(INVALID_HANDLE_VALUE, &security.attributes, PAGE_READWRITE, 0,
                                  static_cast<DWORD>(s.mapping_bytes), name(s, L"Mapping").c_str());
    if (!mapping_ || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        error = "Session already exists or mapping creation failed";
        close();
        return false;
    }
    view_ = MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, s.mapping_bytes);
    if (!view_)
    {
        error = "Cannot map bus";
        close();
        return false;
    }
    event_ = CreateEventW(&security.attributes, FALSE, FALSE, name(s, L"Event").c_str());
    if (!event_ || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        error = "Notification object conflicts";
        close();
        return false;
    }
    std::memset(view_, 0, s.mapping_bytes);
    session_ = s;
    session_.producer_pid = GetCurrentProcessId();
    auto *shared = static_cast<Shared *>(view_);
    shared->header = session_;
    producer_ = true;
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    heartbeat(now.QuadPart);
    store(shared->ready, 1);
    SetEvent(event_);
    return true;
}
bool StageBusWin32::open(const StageBusSessionHeader &expected, std::string &error, void *preferred)
{
    close();
    if (validateSession(expected, expected.mapping_bytes) != StageBusValidationResult::Ok)
    {
        error = "Invalid expected session";
        return false;
    }
    mapping_ = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name(expected, L"Mapping").c_str());
    if (!mapping_)
    {
        error = "Publisher unavailable";
        return false;
    }
    view_ = MapViewOfFileEx(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, expected.mapping_bytes, preferred);
    if (!view_)
    {
        error = "Mapping failed at requested address";
        close();
        return false;
    }
    auto *shared = static_cast<Shared *>(view_);
    if (load(shared->ready) != 1)
    {
        error = "Publisher not ready";
        close();
        return false;
    }
    session_ = shared->header;
    if (validateSession(session_, expected.mapping_bytes) != StageBusValidationResult::Ok ||
        std::memcmp(session_.session_id, expected.session_id, 16) ||
        session_.layout_id != expected.layout_id ||
        session_.qpc_frequency != expected.qpc_frequency || session_.producer_pid == 0)
    {
        error = "Incompatible publisher";
        close();
        return false;
    }
    peer_ = OpenProcess(SYNCHRONIZE, FALSE, session_.producer_pid);
    if (!peer_ || WaitForSingleObject(peer_, 0) != WAIT_TIMEOUT)
    {
        error = "Dead publisher";
        close();
        return false;
    }
    event_ = OpenEventW(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, name(expected, L"Event").c_str());
    if (!event_)
    {
        error = "No notification event";
        close();
        return false;
    }
    if (InterlockedCompareExchange(reinterpret_cast<volatile LONG *>(&shared->header.consumer_pid),
                                   static_cast<LONG>(GetCurrentProcessId()), 0) != 0)
    {
        error = "Consumer already claimed; restart needs a new session";
        close();
        return false;
    }
    producer_ = false;
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    heartbeat(now.QuadPart);
    return true;
}
void StageBusWin32::fault() noexcept
{
    if (view_)
    {
        store(static_cast<Shared *>(view_)->fault, 1);
        SetEvent(event_);
    }
}
bool StageBusWin32::requestPending() const noexcept
{
    if (!view_ || !producer_)
        return true;
    auto *s = static_cast<const Shared *>(view_);
    return load(s->requestWrite) != load(s->requestRead);
}
bool StageBusWin32::writable() const noexcept
{
    if (!view_ || !producer_)
        return false;
    auto *s = static_cast<const Shared *>(view_);
    auto w = load(s->write), r = load(s->read);
    return !load(s->fault) && w >= r && w - r < 4;
}
StageBusIoResult StageBusWin32::tryPublish(const StageBusBlockHeader &b,
                                           const double *const *channels) noexcept
{
    if (!view_ || !producer_)
        return StageBusIoResult::Fault;
    auto *s = static_cast<Shared *>(view_);
    if (load(s->fault))
        return StageBusIoResult::Fault;
    auto w = load(s->write), r = load(s->read);
    if (w < r || w - r > 4 || w == INT64_MAX)
    {
        fault();
        return StageBusIoResult::Fault;
    }
    if (w - r == 4)
    {
        ++full_;
        return StageBusIoResult::Full;
    }
    if (validateBlock(session_, b, session_.slot_stride) != StageBusValidationResult::Ok ||
        b.block_sequence != lastSequence_ + 1 || b.stream_epoch != (lastEpoch_ ? lastEpoch_ : 1) ||
        !channels)
    {
        fault();
        return StageBusIoResult::Fault;
    }
    for (unsigned c = 0; c < session_.channels; ++c)
        if (!channels[c])
        {
            fault();
            return StageBusIoResult::Fault;
        }
    auto *slot = static_cast<unsigned char *>(view_) + session_.slots_offset +
                 (static_cast<uint64_t>(w) & 3) * session_.slot_stride;
    std::memcpy(slot, &b, sizeof(b));
    for (unsigned c = 0; c < session_.channels; ++c)
        std::memcpy(slot + 128 + c * session_.plane_stride, channels[c], session_.plane_stride);
    lastSequence_ = b.block_sequence;
    lastEpoch_ = b.stream_epoch;
    store(s->write, w + 1);
    SetEvent(event_);
    return StageBusIoResult::Ok;
}
StageBusIoResult StageBusWin32::tryConsume(StageBusBlockHeader &b, double *const *channels) noexcept
{
    if (!view_ || producer_)
        return StageBusIoResult::Fault;
    auto *s = static_cast<Shared *>(view_);
    if (load(s->fault))
        return StageBusIoResult::Fault;
    auto r = load(s->read), w = load(s->write);
    if (w < r || w - r > 4 || r == INT64_MAX)
    {
        fault();
        return StageBusIoResult::Fault;
    }
    if (w == r)
    {
        ++empty_;
        return StageBusIoResult::Empty;
    }
    auto *slot = static_cast<unsigned char *>(view_) + session_.slots_offset +
                 (static_cast<uint64_t>(r) & 3) * session_.slot_stride;
    StageBusBlockHeader local{};
    std::memcpy(&local, slot, sizeof(local));
    if (validateBlock(session_, local, session_.slot_stride) != StageBusValidationResult::Ok ||
        local.block_sequence != lastSequence_ + 1 ||
        local.stream_epoch != (lastEpoch_ ? lastEpoch_ : 1) || !channels)
    {
        fault();
        return StageBusIoResult::Fault;
    }
    for (unsigned c = 0; c < session_.channels; ++c)
        if (!channels[c])
        {
            fault();
            return StageBusIoResult::Fault;
        }
    for (unsigned c = 0; c < session_.channels; ++c)
        std::memcpy(channels[c], slot + 128 + c * session_.plane_stride, session_.plane_stride);
    b = local;
    lastSequence_ = b.block_sequence;
    lastEpoch_ = b.stream_epoch;
    store(s->read, r + 1);
    SetEvent(event_);
    return StageBusIoResult::Ok;
}
StageBusIoResult StageBusWin32::trySendRequest(const StageBusFrameRequest &request) noexcept
{
    if (!view_ || producer_)
        return StageBusIoResult::Fault;
    auto *s = static_cast<Shared *>(view_);
    if (load(s->fault))
        return StageBusIoResult::Fault;
    if (validateRequest(session_, request) != StageBusValidationResult::Ok)
    {
        fault();
        return StageBusIoResult::Fault;
    }
    auto w = load(s->requestWrite), r = load(s->requestRead);
    if (w != r)
    {
        ++full_;
        return StageBusIoResult::Full;
    }
    if (w == INT64_MAX)
    {
        fault();
        return StageBusIoResult::Fault;
    }
    s->request = request;
    store(s->requestWrite, w + 1);
    SetEvent(event_);
    return StageBusIoResult::Ok;
}
StageBusIoResult StageBusWin32::tryReceiveRequest(StageBusFrameRequest &request) noexcept
{
    if (!view_ || !producer_)
        return StageBusIoResult::Fault;
    auto *s = static_cast<Shared *>(view_);
    if (load(s->fault))
        return StageBusIoResult::Fault;
    auto r = load(s->requestRead), w = load(s->requestWrite);
    if (w == r)
        return StageBusIoResult::Empty;
    if (w - r != 1)
    {
        fault();
        return StageBusIoResult::Fault;
    }
    auto local = s->request;
    if (validateRequest(session_, local) != StageBusValidationResult::Ok)
    {
        fault();
        return StageBusIoResult::Fault;
    }
    request = local;
    store(s->requestRead, r + 1);
    return StageBusIoResult::Ok;
}
StageBusIoResult StageBusWin32::trySendSourceRequest(const StageBusFrameRequest &request) noexcept
{
    if (!view_ || producer_)
        return StageBusIoResult::Fault;
    auto *s = static_cast<Shared *>(view_);
    if (load(s->fault))
        return StageBusIoResult::Fault;
    if (validateSourceDemandRequest(session_, request) != StageBusValidationResult::Ok)
    {
        fault();
        return StageBusIoResult::Fault;
    }
    auto w = load(s->requestWrite), r = load(s->requestRead);
    if (w != r)
    {
        ++full_;
        return StageBusIoResult::Full;
    }
    if (w == INT64_MAX)
    {
        fault();
        return StageBusIoResult::Fault;
    }
    s->request = request;
    store(s->requestWrite, w + 1);
    SetEvent(event_);
    return StageBusIoResult::Ok;
}
StageBusIoResult StageBusWin32::tryReceiveSourceRequest(StageBusFrameRequest &request) noexcept
{
    if (!view_ || !producer_)
        return StageBusIoResult::Fault;
    auto *s = static_cast<Shared *>(view_);
    if (load(s->fault))
        return StageBusIoResult::Fault;
    auto r = load(s->requestRead), w = load(s->requestWrite);
    if (w == r)
        return StageBusIoResult::Empty;
    if (w - r != 1)
    {
        fault();
        return StageBusIoResult::Fault;
    }
    auto local = s->request;
    if (validateSourceDemandRequest(session_, local) != StageBusValidationResult::Ok)
    {
        fault();
        return StageBusIoResult::Fault;
    }
    request = local;
    store(s->requestRead, r + 1);
    return StageBusIoResult::Ok;
}
void StageBusWin32::heartbeat(uint64_t now) noexcept
{
    if (view_)
    {
        auto *s = static_cast<Shared *>(view_);
        store(producer_ ? s->producerHeartbeat : s->consumerHeartbeat, now);
    }
}
bool StageBusWin32::peerAlive(uint64_t now) const noexcept
{
    if (!view_)
        return false;
    auto *s = static_cast<const Shared *>(view_);
    if (load(s->fault))
        return false;
    HANDLE handle = static_cast<HANDLE>(peer_);
    bool temporary = false;
    if (producer_)
    {
        auto pid = InterlockedCompareExchange(
            const_cast<volatile LONG *>(
                reinterpret_cast<const volatile LONG *>(&s->header.consumer_pid)),
            0, 0);
        if (!pid)
            return false;
        handle = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
        temporary = true;
    }
    if (!handle)
        return false;
    bool alive = WaitForSingleObject(handle, 0) == WAIT_TIMEOUT;
    if (temporary)
        CloseHandle(handle);
    auto last = load(producer_ ? s->consumerHeartbeat : s->producerHeartbeat);
    if (now && (last <= 0 || now < static_cast<uint64_t>(last) ||
                now - static_cast<uint64_t>(last) > session_.qpc_frequency))
        alive = false;
    return alive;
}
} // namespace entropy::stage

namespace entropy::stage
{
StageBusIoResult StageBusWin32::trySendResponse(const StageBusBlockHeader &response) noexcept
{
    if (!view_ || !producer_)
        return StageBusIoResult::Fault;
    auto *shared = static_cast<Shared *>(view_);
    if (load(shared->fault))
        return StageBusIoResult::Fault;
    if (validateBlock(session_, response, session_.slot_stride) != StageBusValidationResult::Ok)
    {
        fault();
        return StageBusIoResult::Fault;
    }
    auto w = load(shared->responseWrite), r = load(shared->responseRead);
    if (w != r)
        return StageBusIoResult::Full;
    if (w == INT64_MAX)
    {
        fault();
        return StageBusIoResult::Fault;
    }
    shared->response = response;
    store(shared->responseWrite, w + 1);
    SetEvent(event_);
    return StageBusIoResult::Ok;
}
StageBusIoResult StageBusWin32::tryReceiveResponse(StageBusBlockHeader &response) noexcept
{
    if (!view_ || producer_)
        return StageBusIoResult::Fault;
    auto *shared = static_cast<Shared *>(view_);
    if (load(shared->fault))
        return StageBusIoResult::Fault;
    auto w = load(shared->responseWrite), r = load(shared->responseRead);
    if (w == r)
        return StageBusIoResult::Empty;
    if (w - r != 1)
    {
        fault();
        return StageBusIoResult::Fault;
    }
    auto local = shared->response;
    if (validateBlock(session_, local, session_.slot_stride) != StageBusValidationResult::Ok)
    {
        fault();
        return StageBusIoResult::Fault;
    }
    response = local;
    store(shared->responseRead, r + 1);
    return StageBusIoResult::Ok;
}
} // namespace entropy::stage

namespace entropy::stage
{
namespace
{
bool validSourceControl(const StageBusSessionHeader &session,
                        const StageBusSourceControl &s) noexcept
{
    return !std::memcmp(session.session_id, s.session_id, 16) && s.source_revision &&
           s.manual_mute_mask <= 7 && s.music_left != s.music_right && !s.reserved;
}
} // namespace
StageBusIoResult StageBusWin32::trySendSourceControl(const StageBusSourceControl &control) noexcept
{
    if (!view_ || !producer_)
        return StageBusIoResult::Fault;
    auto *shared = static_cast<Shared *>(view_);
    if (load(shared->fault))
        return StageBusIoResult::Fault;
    if (!validSourceControl(session_, control))
    {
        fault();
        return StageBusIoResult::Fault;
    }
    auto w = load(shared->sourceControlWrite), r = load(shared->sourceControlRead);
    if (w != r)
        return StageBusIoResult::Full;
    if (w == INT64_MAX)
    {
        fault();
        return StageBusIoResult::Fault;
    }
    shared->sourceControl = control;
    store(shared->sourceControlWrite, w + 1);
    SetEvent(event_);
    return StageBusIoResult::Ok;
}
StageBusIoResult StageBusWin32::tryReceiveSourceControl(StageBusSourceControl &control) noexcept
{
    if (!view_ || producer_)
        return StageBusIoResult::Fault;
    auto *shared = static_cast<Shared *>(view_);
    if (load(shared->fault))
        return StageBusIoResult::Fault;
    auto w = load(shared->sourceControlWrite), r = load(shared->sourceControlRead);
    if (w == r)
        return StageBusIoResult::Empty;
    if (w - r != 1)
    {
        fault();
        return StageBusIoResult::Fault;
    }
    auto local = shared->sourceControl;
    if (!validSourceControl(session_, local))
    {
        fault();
        return StageBusIoResult::Fault;
    }
    control = local;
    store(shared->sourceControlRead, r + 1);
    return StageBusIoResult::Ok;
}
} // namespace entropy::stage
