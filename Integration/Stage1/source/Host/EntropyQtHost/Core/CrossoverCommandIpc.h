#pragma once
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
namespace entropy::host {
struct CrossoverIpcParameter {
    std::uint64_t sourceSequence=0;
    std::uint32_t action=0;
    double value=0;
    std::array<char,128> identifier{};
};
// Versioned optional IPC extension. Existing SharedControl v6 remains unchanged.
// Engine maps once before its producer starts; its pop is bounded and allocation-free.
// UI mapping/name construction happens in sender outside the realtime owner.
class CrossoverCommandIpc {
public:
    static constexpr LONG capacity=256;
    enum class SendResult { unavailable, accepted, rejected };
    static std::wstring mappingName(std::uint64_t session) {
        return L"Local\\EntropyAudio.ModularHost.Crossover.Commands.v1."+std::to_wstring(session);
    }
    CrossoverCommandIpc()=default;
    ~CrossoverCommandIpc(){close();}
    CrossoverCommandIpc(const CrossoverCommandIpc&)=delete;
    CrossoverCommandIpc& operator=(const CrossoverCommandIpc&)=delete;
    bool create(std::uint64_t session) noexcept {
        if(!session||shared_)return false;
        try {
            const auto name=mappingName(session);
            handle_=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Shared),name.c_str());
            if(!handle_)return false;
            if(GetLastError()==ERROR_ALREADY_EXISTS){CloseHandle(handle_);handle_=nullptr;return false;}
            shared_=static_cast<Shared*>(MapViewOfFile(handle_,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));
            if(!shared_){CloseHandle(handle_);handle_=nullptr;return false;}
            std::memset(shared_,0,sizeof(Shared));
            shared_->magic=magic;shared_->version=1;shared_->bytes=sizeof(Shared);
            shared_->ownerPid=GetCurrentProcessId();shared_->session=session;
            InterlockedExchange(&shared_->ready,1);return true;
        } catch(...) {close();return false;}
    }
    void close() noexcept {
        if(shared_){InterlockedExchange(&shared_->ready,0);UnmapViewOfFile(shared_);shared_=nullptr;}
        if(handle_){CloseHandle(handle_);handle_=nullptr;}
    }
    bool pop(CrossoverIpcParameter& value) noexcept {
        if(!shared_||InterlockedCompareExchange(&shared_->ready,0,0)!=1)return false;
        const LONG read=load(shared_->read),write=load(shared_->write);
        if(read==write)return false;
        if(read<0||read>capacity||write<0||write>capacity)return false;
        value=shared_->slots[static_cast<std::size_t>(read)];
        InterlockedExchange(&shared_->read,(read+1)%(capacity+1));return true;
    }
    bool empty() const noexcept{return !shared_ || load(shared_->read)==load(shared_->write);}
    // Audio-owner consumer: merge only adjacent obsolete continuous values.
    // BEGIN/END, another identifier and discrete operations remain barriers.
    // A captured write index bounds work even when the UI keeps producing.
    LONG writeCursor() const noexcept{return shared_?load(shared_->write):0;}
    bool drainedThrough(LONG cutoff) const noexcept{return !shared_||load(shared_->read)==cutoff;}
    bool popLatestContinuousSet(CrossoverIpcParameter& value) noexcept {
        return popLatestContinuousSetThrough(value,writeCursor());
    }
    // The saved cursor is a publication boundary. Post-Save edits are never
    // consumed/coalesced into the preceding project generation.
    bool popLatestContinuousSetThrough(CrossoverIpcParameter& value,LONG cutoff) noexcept {
        if(!shared_||cutoff<0||cutoff>capacity||drainedThrough(cutoff))return false;
        if(!pop(value))return false;
        if(value.action!=0 || !continuousIdentifier(value.identifier))return true;
        LONG read=load(shared_->read);
        if(read<0||read>capacity)return true;
        for(LONG inspected=1;inspected<capacity && read!=cutoff;++inspected) {
            const auto& next=shared_->slots[static_cast<std::size_t>(read)];
            if(next.action!=0 || std::memcmp(next.identifier.data(),value.identifier.data(),value.identifier.size())!=0)break;
            value=next;read=(read+1)%(capacity+1);
        }
        InterlockedExchange(&shared_->read,read);return true;
    }
    bool queuedSequence(std::uint64_t sequence) const noexcept {
        return shared_ && sequence!=0 && sequence<=static_cast<std::uint64_t>(InterlockedCompareExchange64(&shared_->lastQueuedSequence,0,0));
    }
    std::uint64_t overflowCount() const noexcept{return shared_?static_cast<std::uint64_t>(InterlockedCompareExchange64(&shared_->overflow,0,0)):0;}
    static SendResult send(std::uint64_t session,const CrossoverIpcParameter& value) noexcept {
        if(!session)return SendResult::unavailable;
        HANDLE handle=nullptr;Shared* shared=nullptr;bool claimed=false;
        auto cleanup=[&]{if(claimed)InterlockedExchange(&shared->producerBusy,0);if(shared)UnmapViewOfFile(shared);if(handle)CloseHandle(handle);};
        try {
            const auto name=mappingName(session);handle=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,name.c_str());
            if(!handle)return GetLastError()==ERROR_FILE_NOT_FOUND?SendResult::unavailable:SendResult::rejected;
            shared=static_cast<Shared*>(MapViewOfFile(handle,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared)));
            if(!shared){cleanup();return SendResult::rejected;}
            const bool valid=load(shared->ready)==1 && shared->magic==magic && shared->version==1 && shared->bytes==sizeof(Shared) && shared->session==session;
            if(!valid){cleanup();return SendResult::rejected;}
            HANDLE owner=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,shared->ownerPid);DWORD status=0;
            const bool ownerAlive=owner && GetExitCodeProcess(owner,&status) && status==STILL_ACTIVE;
            if(owner)CloseHandle(owner);
            if(!ownerAlive){cleanup();return SendResult::rejected;}
            const LONG pid=static_cast<LONG>(GetCurrentProcessId());
            LONG producer=InterlockedCompareExchange(&shared->producerPid,pid,0);
            if(producer!=0&&producer!=pid){cleanup();return SendResult::rejected;}
            const LONG thread=static_cast<LONG>(GetCurrentThreadId());
            const LONG producerThread=InterlockedCompareExchange(&shared->producerThreadId,thread,0);
            if(producerThread!=0 && producerThread!=thread){cleanup();return SendResult::rejected;}
            claimed=InterlockedCompareExchange(&shared->producerBusy,1,0)==0;
            if(!claimed){cleanup();return SendResult::rejected;}
            const LONG write=load(shared->write),read=load(shared->read);
            if(write<0||write>capacity||read<0||read>capacity){cleanup();return SendResult::rejected;}
            const LONG next=(write+1)%(capacity+1);
            if(next==read){InterlockedIncrement64(&shared->overflow);cleanup();return SendResult::rejected;}
            shared->slots[static_cast<std::size_t>(write)]=value;
            InterlockedExchange64(&shared->lastQueuedSequence,static_cast<LONG64>(value.sourceSequence));
            InterlockedExchange(&shared->write,next);
            const bool alive=load(shared->ready)==1;
            cleanup();return alive?SendResult::accepted:SendResult::rejected;
        } catch(...) {cleanup();return SendResult::rejected;}
    }
private:
    static bool continuousIdentifier(const std::array<char,128>& id) noexcept {
        constexpr const char* prefixes[]{"gainDb","musicGainDb","cinemaGainDb","musicTrimDb","cinemaTrimDb","musicDelayMs","cinemaDelayMs"};
        for(const auto* prefix:prefixes) {
            const auto n=std::strlen(prefix);
            if(std::strncmp(id.data(),prefix,n)!=0)continue;
            unsigned channel=0;std::size_t i=n;
            for(;i<n+2 && id[i]>='0' && id[i]<='9';++i)channel=channel*10+unsigned(id[i]-'0');
            if(i>n && id[i]=='\0' && channel>=1 && channel<=16)return true;
        }
        return false;
    }
    static constexpr std::uint32_t magic=0x36514358u;
    static LONG load(volatile LONG& value) noexcept{return InterlockedCompareExchange(&value,0,0);}
    struct Shared {
        std::uint32_t magic,version,bytes,ownerPid;
        std::uint64_t session;
        volatile LONG ready,producerPid,producerThreadId,producerBusy,read,write;
        alignas(8) volatile LONG64 lastQueuedSequence,overflow;
        std::array<CrossoverIpcParameter,capacity+1> slots;
    };
    static_assert(std::is_trivially_copyable_v<CrossoverIpcParameter>);
    HANDLE handle_=nullptr;Shared* shared_=nullptr;
};
}
