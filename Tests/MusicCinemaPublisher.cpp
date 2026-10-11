#include "MusicCinemaFixture.h"
int main(int argc,char** argv) try {unsigned layout=argc>1?std::stoul(argv[1]):4;unsigned id=argc>2?std::stoul(argv[2]):21;unsigned period=argc>3?std::stoul(argv[3]):5;Source s(layout,id,0,1.0,period);std::wcout<<s.endpoint<<std::endl;Sleep(30000);return 0;}catch(...){return 1;}
