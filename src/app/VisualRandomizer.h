#pragma once
#include <algorithm>
#include <cmath>
#include <random>
#include <unordered_map>
namespace cadence {
struct VisualRandomBaseline {
    std::unordered_map<float*,float> floats;
    std::unordered_map<bool*,bool> flags;
    float original(float& v){return floats.try_emplace(&v,v).first->second;}
    bool original(bool& v){return flags.try_emplace(&v,v).first->second;}
    void restore()const{for(auto [p,v]:floats)*p=v;for(auto [p,v]:flags)*p=v;}
    bool empty()const{return floats.empty()&&flags.empty();}
    void clear(){floats.clear();flags.clear();}
};
inline float randomizedVisual(float current,float low,float high,float similarity,std::mt19937& rng){
    const float keep=std::clamp(similarity*.01f,0.f,1.f);if(keep>=1)return current;
    const float target=std::uniform_real_distribution<float>(low,high)(rng);
    if(keep<=0||!std::isfinite(current))return target;
    return current*keep+target*(1-keep);
}
}
