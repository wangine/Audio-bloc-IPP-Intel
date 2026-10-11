#include "TestSupport.h"
#include "DspRecorder.h"
#include <fstream>
int main(){
 auto path=std::filesystem::temp_directory_path()/("EntropyMusicRecorder-"+std::to_string(GetCurrentProcessId())+".music3");
 DspRecorder recorder;CHECK(recorder.start(path,2));Planes<11> planes;
 for(unsigned c=0;c<11;c++)for(unsigned f=0;f<1024;f++)planes.data[c][f]=c+.0001*f;
 auto s=session(LRC_CINEMA11_V1);auto a=header(request(s,1)),b=header(request(s,2));
 CHECK(recorder.tryCapture(a,planes.in.data()));CHECK(recorder.tryCapture(b,planes.in.data()));recorder.stop();
 CHECK(!recorder.failed() && recorder.written()==2);
 CHECK(std::filesystem::file_size(path)==2*(128+3*8192));
 std::ifstream file(path,std::ios::binary);StageBusBlockHeader got{};Planes<3> read;
 for(auto expected:{a,b}){file.read(reinterpret_cast<char*>(&got),128);CHECK(!std::memcmp(&got,&expected,128));file.read(reinterpret_cast<char*>(read.data.data()),3*8192);for(unsigned c=0;c<3;c++)CHECK(!std::memcmp(read.in[c],planes.in[c],8192));}
 file.close();std::filesystem::remove(path);std::cout<<"PASS MUSIC3 recorder exact headers/payload and 24704-byte block\n";
}
