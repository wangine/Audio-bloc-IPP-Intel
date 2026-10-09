#include "CinemaWdlSrc.h"
#include <array>
#include <vector>
#include <iostream>
#include <cmath>
using namespace entropy::host;
int main(){for(unsigned taps:{64u,128u})for(unsigned rate:{48000u,96000u})for(unsigned frequency:{1000u,10000u,18000u,20000u,22000u}){CinemaWdlSrc src;if(!src.prepare(256,taps)||!src.configure(rate))return 2;std::array<std::array<double,256>,8>in{};std::array<const double*,8>ip{};std::array<std::vector<double>,8>out;std::array<double*,8>op{};for(unsigned c=0;c<8;++c){ip[c]=in[c].data();out[c].resize(src.outputCapacity());op[c]=out[c].data();}unsigned cursor=0;double desiredSin=0,desiredCos=0,imageSin=0,imageCos=0;for(unsigned first=0;first<8192;first+=256){for(unsigned i=0;i<256;++i)for(unsigned c=0;c<8;++c)in[c][i]=0.1*std::sin(6.283185307179586*frequency*(first+i)/rate);unsigned n;if(!src.process(ip.data(),256,op.data(),src.outputCapacity(),n))return 2;for(unsigned i=0;i<n;++i,++cursor)if(cursor>=1024&&cursor<7168){double phase=6.283185307179586*frequency*cursor/192000,image=6.283185307179586*(rate-frequency)*cursor/192000;desiredSin+=out[0][i]*std::sin(phase);desiredCos+=out[0][i]*std::cos(phase);imageSin+=out[0][i]*std::sin(image);imageCos+=out[0][i]*std::cos(image);}}std::cout<<taps<<','<<rate<<','<<frequency<<','<<20*std::log10(2*std::hypot(desiredSin,desiredCos)/6144/0.1)<<','<<20*std::log10(2*std::hypot(imageSin,imageCos)/6144/0.1)<<'\n';}}
