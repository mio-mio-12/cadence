#pragma once
#include "scene/CastScene.h"
#include <algorithm>
#include <chrono>

namespace render {
class VisibilityPreparation {
    std::vector<std::int32_t> parents_;
    std::unordered_set<std::size_t> hidden_;
    std::size_t outputSize_{static_cast<std::size_t>(-1)};
public:
    // Exact structural snapshots survive scene replacement, replay seeks and
    // same-size edits. Pose matrices do not affect bone visibility.
    bool update(const scene::Skeleton& skeleton,const std::unordered_set<std::size_t>& hidden,std::size_t size,std::vector<float>& shown){
        const auto count=std::min(size,skeleton.bones.size());
        bool same=outputSize_==size&&parents_.size()==skeleton.bones.size()&&hidden_==hidden;
        if(same)for(std::size_t i=0;i<parents_.size();++i)if(parents_[i]!=skeleton.bones[i].parent){same=false;break;}
        if(same)return false;
        outputSize_=size;parents_.resize(skeleton.bones.size());
        for(std::size_t i=0;i<parents_.size();++i)parents_[i]=skeleton.bones[i].parent;
        hidden_=hidden;bool changed=shown.size()!=size;shown.resize(size,1.f);
        for(std::size_t i=0;i<size;++i){
            float value=1.f;
            if(i<count&&!hidden.empty()){
                auto bone=static_cast<std::int32_t>(i);
                for(std::size_t guard=0;bone>=0&&static_cast<std::size_t>(bone)<parents_.size()&&guard<parents_.size();++guard){
                    if(hidden.contains(static_cast<std::size_t>(bone))){value=0;break;}bone=parents_[bone];
                }
            }
            if(shown[i]!=value)changed=true;shown[i]=value;
        }
        return changed;
    }
};
struct SurfaceSortKey {scene::Vec3 center{};int queue{2000};bool explicitPolicy{};};
template<class Meshes>
void sortTransparentPass(std::vector<std::size_t>& pass,const Meshes& meshes,scene::Vec3 camera,std::vector<float>& distances){
    if(std::none_of(pass.begin(),pass.end(),[&](std::size_t i){return meshes[i].sortKey.explicitPolicy;}))return;
    distances.resize(meshes.size());
    for(const auto i:pass){const auto d=meshes[i].sortKey.center-camera;distances[i]=scene::dot(d,d);}
    std::stable_sort(pass.begin(),pass.end(),[&](std::size_t a,std::size_t b){
        const auto aq=meshes[a].sortKey.queue,bq=meshes[b].sortKey.queue;
        return aq!=bq?aq<bq:distances[a]>distances[b];
    });
}
// Used for readout-only GPU memory polling; counters remain current each frame.
struct DriverPollInterval {
    using Clock=std::chrono::steady_clock;
    Clock::time_point last{};bool sampled{};
    bool due(Clock::time_point now){if(sampled&&now-last<std::chrono::seconds(1))return false;sampled=true;last=now;return true;}
};
}
