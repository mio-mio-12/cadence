#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace cadence::replay {
// IWXMVM-style controls, implemented independently. No live input is changed.
inline constexpr std::array<float,20> speeds{.001f,.005f,.01f,.025f,.05f,.1f,.125f,.2f,.25f,.333f,.5f,.75f,1,1.25f,1.5f,2,5,10,20,50};
inline float speedStep(float current,bool faster){if(faster){for(float s:speeds)if(s>current+.000001f)return s;return speeds.back();}for(auto i=speeds.rbegin();i!=speeds.rend();++i)if(*i<current-.000001f)return *i;return speeds.front();}
inline float seek(float current,bool forward,float duration,float start,float end){float target=std::clamp(current+(forward?1.f:-1.f),0.f,std::max(0.f,duration));for(float marker:{start,end})if(forward?marker>current+.0001f&&marker<target:marker<current-.0001f&&marker>target)target=marker;return target;}
}
