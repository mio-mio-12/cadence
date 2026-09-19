#pragma once
#include "scene/Math3D.h"
#include <vector>
#include <cstdint>

namespace render {
struct SmokeCurvePoint {
    scene::Vec3 position{},normal{},velocity{};
    float age{},lifetime{1},startWidth{1},endWidth{4},taperingExp{1};
};
struct ReplaySmokeSettings {
    float emissionDuration{.35f},lifetime{1.1f},startWidth{3.41f},endWidth{20},taperingExp{1.44f},blastSpeed{30.7f},riseSpeed{50},dispersion{2.6f};
};
// Absolute-time reconstruction: no frame history, wall clock, global RNG or
// recorded styling. Bounded output and work even for long takes/large settings.
inline void sampleReplaySmoke(std::vector<SmokeCurvePoint>& out,scene::Vec3 origin,scene::Vec3 forward,float age,std::uint32_t seed,const ReplaySmokeSettings& s){
    out.clear();if(!std::isfinite(age)||age<0)return;
    const float duration=std::max(0.f,s.emissionDuration),life=std::max(.1f,s.lifetime),tail=1.f/6.f;
    if(age>duration+tail+life)return;
    forward=scene::length(forward)>.001f?scene::normalize(forward):scene::Vec3{1,0,0};
    const auto side=scene::normalize(scene::cross(std::abs(forward.z)<.95f?scene::Vec3{0,0,1}:scene::Vec3{0,1,0},forward));
    const auto up=scene::normalize(scene::cross(forward,side));
    const float end=std::min(age,duration+tail),begin=std::max(0.f,age-life);
    const float interval=std::max(.025f,(duration+tail)/255.f);
    const int first=std::max(0,static_cast<int>(std::ceil(begin/interval))),last=std::min(255,static_cast<int>(std::floor(end/interval)));
    for(int i=first;i<=last;++i){
        const float emission=i*interval,t=age-emission;
        const float taper=emission<=duration?std::min(1.f,emission*8.f):std::max(0.f,std::min(1.f,duration*8.f)-(emission-duration)*6.f);
        if(t>=life||taper<=0)continue;
        std::uint32_t hash=seed^(std::uint32_t(i)*0x9e3779b9u);hash^=hash>>16;hash*=0x7feb352du;hash^=hash>>15;
        const float angle=(hash&65535u)*(2.f*scene::kPi/65536.f);
        const auto normal=side*std::cos(angle)+up*std::sin(angle);
        const auto v=forward*(s.blastSpeed*(emission>duration?.4f:1.f))+scene::Vec3{0,0,s.riseSpeed};
        const float decay=(1.f-std::exp(-2.2f*t))/2.2f;
        SmokeCurvePoint p;p.position=origin+v*decay+scene::Vec3{0,0,12.f}*((t-decay)/2.2f)+normal*(s.dispersion*t);
        p.normal=normal;p.age=t;p.lifetime=life;p.startWidth=s.startWidth*taper;p.endWidth=s.endWidth*(emission>duration?taper:1.f);p.taperingExp=s.taperingExp;out.push_back(p);
    }
}
}
