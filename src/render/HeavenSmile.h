#pragma once
#include "scene/Math3D.h"
#include <vector>
#include <cstdint>
#include <algorithm>
namespace render::heaven_smile {
struct Burst {scene::Vec3 center;float age{},height{170};std::uint32_t seed{};};
struct Shard {scene::Vec3 center,direction;float radius,alpha;};
struct StreamSegment {scene::Vec3 start,end;float radius;};
inline std::uint32_t hash(std::uint32_t n){n^=n>>16;n*=0x7feb352d;n^=n>>15;n*=0x846ca68b;return n^(n>>16);}
inline float random(std::uint32_t n){return (hash(n)&0xffffff)/16777216.f;}
// Analytic trajectories keep playback/seek independent of frame history.
inline std::vector<StreamSegment> streams(const Burst& b){
    std::vector<StreamSegment> result;
    if(!std::isfinite(b.age)||b.age<0||b.age>=1.7f)return result;
    const float h=std::clamp(b.height,20.f,600.f);
    for(unsigned jet=0;jet<12;++jet){
        const auto seed=b.seed*7919+jet*104729;
        const float age=b.age-random(seed+3)*.18f;if(age<=0)continue;
        const float angle=random(seed)*2*scene::kPi;
        const scene::Vec3 velocity{std::cos(angle)*h*.6f,std::sin(angle)*h*.6f,h*(.55f+random(seed+1))};
        const auto origin=b.center+scene::Vec3{0,0,(random(seed+2)-.5f)*h*.5f};
        const auto position=[&](float t){return origin+velocity*t+scene::Vec3{0,0,-h*.8f*t*t};};
        const float tail=std::max(0.f,age-.24f),width=h*.008f*std::clamp((1.7f-b.age)/.4f,0.f,1.f);
        for(int k=0;k<10;++k){const float a=tail+(age-tail)*k/10.f,c=tail+(age-tail)*(k+1)/10.f;
            result.push_back({position(a),position(c),width*(.3f+.7f*(k+1)/10.f)});
        }
    }return result;
}
inline std::vector<Shard> shards(const Burst& b){
    std::vector<Shard> result;if(!std::isfinite(b.age)||b.age<0||b.age>=1.7f)return result;
    result.reserve(96);const float h=std::clamp(b.height,20.f,600.f);
    for(unsigned i=0;i<96;++i){const auto seed=b.seed*7919+i*104729;
        const float angle=random(seed)*scene::kPi*2,z=random(seed+1)*2-1;
        const float r=std::sqrt(std::max(0.f,1-z*z));const scene::Vec3 direction{std::cos(angle)*r,std::sin(angle)*r,z};
        const float delay=random(seed+5)*.16f,t=std::max(0.f,b.age-delay);
        const float distance=(.15f+random(seed+2)*.7f)*h*(1-std::exp(-t*3));
        const auto p=b.center+scene::Vec3{0,0,(random(seed+3)-.5f)*h*.65f}+direction*distance+scene::Vec3{0,0,-h*.22f*t*t};
        result.push_back({p,direction,h*(.012f+random(seed+4)*.028f)*(1-.4f*t/1.7f),std::clamp((1.7f-b.age)/.65f,0.f,1.f)});
    }return result;
}
}
