#pragma once
#include <cmath>
namespace entropy::visual {
inline double frequencyUnit(double hz,double minimum,double maximum)noexcept{return std::log(hz/minimum)/std::log(maximum/minimum);}
inline double frequencyAt(double unit,double minimum,double maximum)noexcept{return minimum*std::exp(unit*std::log(maximum/minimum));}
}
