#include "DspRecorder.h"
#include "TestSupport.h"
#include <filesystem>
#include <fstream>
#include <condition_variable>
#include <mutex>
namespace entropy::stage {struct RecorderRaceProbe {std::mutex mutex;std::condition_variable cv;bool paused=false,resume=false;
 static void pause(void* opaque){auto& p=*static_cast<RecorderRaceProbe*>(opaque);std::unique_lock lock(p.mutex);if(p.paused)return;p.paused=true;p.cv.notify_all();p.cv.wait(lock,[&]{return p.resume;});}
 void arm(DspRecorder& recorder){recorder.emptyQueueProbe_=pause;recorder.probeContext_=this;}
};}
int main(){auto racePath=std::filesystem::temp_directory_path()/("stage2-recorder-race-"+std::to_string(qpc()));DspRecorder race;RecorderRaceProbe probe;probe.arm(race);CHECK(race.start(racePath,1));{std::unique_lock lock(probe.mutex);CHECK(probe.cv.wait_for(lock,std::chrono::seconds(2),[&]{return probe.paused;}));}Planes<11> finalPlanes;auto finalHeader=header(request(session(2),1));CHECK(race.tryCapture(finalHeader,finalPlanes.in.data()));{std::lock_guard lock(probe.mutex);probe.resume=true;probe.cv.notify_all();}Sleep(20);race.stop();CHECK(race.written()==1 && race.dropped()==0);CHECK(std::filesystem::file_size(racePath)==128+11*8192);
auto root=std::filesystem::temp_directory_path()/("stage2-recorder-"+std::to_string(qpc()));std::filesystem::create_directories(root);auto path=root/"audio.bin";DspRecorder recorder;CHECK(!recorder.start(path,0));CHECK(recorder.start(path,16,30));Planes<11> a;for(unsigned c=0;c<11;c++)for(auto& x:a.data[c])x=c+0.25;auto s=session(2);uint64_t offered=0;for(uint64_t seq=1;seq<=16;seq++){auto b=header(request(s,seq));offered+=recorder.tryCapture(b,a.in.data());}recorder.stop();CHECK(recorder.written()==offered && recorder.dropped()==16-offered && recorder.dropped()>0);CHECK(std::filesystem::file_size(path)==offered*(128+11*8192));std::ifstream file(path,std::ios::binary);StageBusBlockHeader b{};Planes<11> read;file.read(reinterpret_cast<char*>(&b),128);file.read(reinterpret_cast<char*>(read.data.data()),11*8192);CHECK(b.block_sequence==1);CHECK(!std::memcmp(read.data.data(),a.data.data(),sizeof(a.data)));CHECK(!recorder.failed());std::cout<<"PASS manual bounded 11ch recorder payload/header, drops separate, STOP drains owned slots; written="<<offered<<" drops="<<recorder.dropped()<<"\n";}
