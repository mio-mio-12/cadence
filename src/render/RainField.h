#pragma once
#include "render/Rain.h"
#include "scene/GlbMap.h"
#include <map>
#include <climits>
#include <utility>

namespace render::rain {
// Deterministic wind-space lanes only. Shelter is rasterized on the GPU: no
// main-thread collision rays, collision-index mutation, or triangle scans.
struct Field {
    struct Lane {scene::Vec4 base;scene::Vec4 normal;};
    std::vector<Lane> lanes;
    std::map<std::pair<int,int>,Lane> cache;
    const scene::glb::Map* map{};
    std::uint64_t revision{};
    int seed{-1}, quality{-1}, cx{INT_MIN},cy{INT_MIN};
    float radius{},wind{},direction{},speed{},top{},bottom{};
    bool shelter{};
    std::size_t queries{};
    bool update(const Settings& s,scene::Vec3 camera,const scene::glb::Map* source){
        queries=0;
        const auto rev=source?source->collisionRevision:0;
        const bool reset=map!=source||revision!=rev||seed!=s.seed||quality!=s.quality||radius!=s.radius
            ||wind!=s.wind||direction!=s.direction||speed!=s.speed||shelter!=s.shelter;
        if(reset){
            map=source;revision=rev;seed=s.seed;quality=s.quality;radius=s.radius;wind=s.wind;direction=s.direction;speed=s.speed;shelter=s.shelter;
            cache.clear();cx=cy=INT_MIN;top=-1e8f;bottom=1e8f;
        }
        const int half=s.quality==0?16:s.quality==1?24:32;
        const float spacing=safeRadius(s)*100/half;
        const auto drift=slope(s);
        // Lane coordinates describe their intersection with the world Z=0 plane.
        const int x=int(std::clamp(std::floor(double(camera.x+drift.x*camera.z)/spacing),-1e8,1e8));
        const int y=int(std::clamp(std::floor(double(camera.y+drift.y*camera.z)/spacing),-1e8,1e8));
        if(x==cx&&y==cy)return false;
        cx=x;cy=y;lanes.clear();lanes.reserve((half*2+1)*(half*2+1));
        for(auto it=cache.begin();it!=cache.end();)if(std::abs(it->first.first-x)>half||std::abs(it->first.second-y)>half)it=cache.erase(it);else ++it;
        for(int j=y-half;j<=y+half;++j)for(int i=x-half;i<=x+half;++i){
            const auto key=std::make_pair(i,j);auto found=cache.find(key);
            if(found==cache.end()){
                const auto h=hash(std::uint32_t(i)*73856093u^std::uint32_t(j)*19349663u^std::uint32_t(seed));
                Lane l{{(i+random(h))*spacing,(j+random(h+1))*spacing,-1e8f,random(h+2)},{0,0,1,random(h+3)}};
                found=cache.emplace(key,l).first;
            }
            lanes.push_back(found->second);
        }
        return true;
    }
};
}
