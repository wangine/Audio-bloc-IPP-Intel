#include "SourceSelectionController.h"
#include <entropy/endpoint_catalog_abi.h>
#include <iostream>
#include <windows.h>
#include <thread>
#include <atomic>
#define CHECK(x)                                                                                   \
    do                                                                                             \
    {                                                                                              \
        if (!(x))                                                                                  \
        {                                                                                          \
            std::cerr << "FAIL " << __LINE__ << ": " << #x << "\n";                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
using namespace entropy::host;
int sourceSelectionProof()
{
    auto key = SourceStartupPolicy::musicKey();
    CHECK(key.providerId == "audio.entropy.endpoint.music-transport" &&
          key.endpointId == "entropy-music-transport:main" &&
          key.accessMode == ENTROPY_ACCESS_MUSIC_TRANSPORT);
    CHECK(SourceStartupPolicy::permitted(key));
    auto wrong = key;
    wrong.endpointId = "default-windows";
    CHECK(!SourceStartupPolicy::permitted(wrong));
    InputSourceSnapshot snapshot;
    InputSourceEntry first;
    first.key = key;
    first.name = "identical name";
    first.selectable = true;
    snapshot.entries.push_back(first);
    auto manual = first;
    manual.key = {"audio.entropy.endpoint.tidal", "actual-catalog-endpoint",
                  ENTROPY_ACCESS_PRIVATE_TAP};
    snapshot.entries.push_back(manual);
    CHECK(findInputSource(snapshot, key).status == InputSourceFindStatus::found);
    CHECK(SourceStartupPolicy::permitted(manual.key));
    snapshot.entries.push_back(first);
    CHECK(findInputSource(snapshot, key).status == InputSourceFindStatus::ambiguous);
    SourceDomainRuntime domain;
    SourceSelectionController selector(domain, TEST_MODULE_ROOT);
    std::string error;
    selector.catalog(snapshot);
    CHECK(!selector.selectMusic(key, error) && error.find("ambiguous") != std::string::npos);
    CHECK(!selector.selectMusic(key, error) && error.find("ambiguous") != std::string::npos);
    snapshot.entries.clear();
    selector.catalog(snapshot);
    CHECK(!selector.selectMusic(key, error) && error.find("absent") != std::string::npos);
    SourceDomainTelemetry failed;
    failed.capture_failed = true;
    failed.state = 2;
    CHECK(SourceSelectionController::needsRetry(true, failed));
    failed.capture_failed = false;
    CHECK(!SourceSelectionController::needsRetry(true, failed));
    failed.state = 0;
    CHECK(SourceSelectionController::needsRetry(true, failed));
    first.selectable = false;
    snapshot.entries.push_back(first);
    selector.catalog(snapshot);
    CHECK(!selector.selectMusic(key, error) &&
          error.find("unavailable") != std::string::npos);
    CHECK(!domain.musicPrepared() && !domain.auxPrepared() && selector.revision() == 0);
    snapshot.entries[0].selectable = true;
    selector.catalog(snapshot);
    selector.leftChannel = 1;
    selector.rightChannel = 0;
    CHECK(!selector.selectMusic(key, error) && error.find("channels") != std::string::npos);
    CHECK(selector.revision() == 0 && !domain.musicPrepared());
    SourceDomainRuntime syntheticDomain;
    SourceSelectionController synthetic(syntheticDomain, TEST_SELECTION_FIXTURE_ROOT);
    InputSourceSnapshot syntheticCatalog;first.selectable=true;syntheticCatalog.entries.push_back(first);
    synthetic.catalog(syntheticCatalog);
    CHECK(synthetic.selectMusic(key,error));
    CHECK(synthetic.revision()==1 && syntheticDomain.musicSessionRevision()==1);
    SourceBlock1024 block;
    auto until=GetTickCount64()+3000;
    while(!syntheticDomain.popMusic1024(block)){CHECK(GetTickCount64()<until);Sleep(1);}
    CHECK(block.sessionRevision==1);
    std::atomic<bool> stopReader{false};
    std::thread reader([&]{SourceBlock1024 b;while(!stopReader){syntheticDomain.popMusic1024(b);Sleep(1);}});
    const bool switched=synthetic.selectMusic(key,error);
    stopReader=true;reader.join();CHECK(switched);
    CHECK(synthetic.revision()==2 && syntheticDomain.musicSessionRevision()==2);
    until=GetTickCount64()+3000;
    while(!syntheticDomain.popMusic1024(block)){CHECK(GetTickCount64()<until);Sleep(1);}
    CHECK(block.sessionRevision==2);
    syntheticDomain.stop();
    std::cout << "PASS exact startup identity, duplicate names/keys, absent provider, local lifecycle, reader-safe rebind/resume, no "
                 "fallback/no ownership side effects\n";
return 0;
}

#ifndef ENTROPY_SOURCE_INDEPENDENCE_AGGREGATE
int main(){return sourceSelectionProof();}
#endif
