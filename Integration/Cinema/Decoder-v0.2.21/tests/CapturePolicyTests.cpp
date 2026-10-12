#include <windows.h>
#include "CapturePolicy.h"
#include "EntropySurroundCore.h"
#include <iostream>
int main(){
 if(!validStereoSilence(true,false,false,AUDCLNT_BUFFERFLAGS_SILENT)){std::cerr<<"FAIL real stereo silence rejected\n";return 1;}
 if(!stereoTransportDiscontinuity(false,AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)||stereoTransportDiscontinuity(false,0)||stereoTransportDiscontinuity(true,AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY))return 1;
 if(validStereoSilence(false,false,false,AUDCLNT_BUFFERFLAGS_SILENT)||validStereoSilence(true,true,true,AUDCLNT_BUFFERFLAGS_SILENT)||validStereoSilence(true,false,false,AUDCLNT_BUFFERFLAGS_SILENT|AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)||validStereoSilence(true,false,false,0))return 1;
 entropy::surround::MatrixSurround a,b;for(int i=0;i<900;i++){a.process(.1f,-.1f,true);b.process(.1f,-.1f,true);}for(int i=0;i<1200;i++)if(a.process(0,0,true)!=b.process(0,0,true))return 1;
 std::cout<<"PASS real SILENT stereo packet eligibility; encoded padding, unconfirmed signal and discontinuity rejected; unchanged upmixer tail\n";return 0;
}
