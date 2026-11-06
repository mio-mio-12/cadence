#pragma once
#include "gameplay/IwPresentation.h"
#include "scene/TestCourse.h"
#include <optional>
#include <vector>
#include <cstdint>
namespace gameplay::bot {
struct SpawnHistory {std::optional<scene::Vec3> last;std::uint64_t sequence{};};
template<class Project,class Occupied>
std::optional<scene::Vec3> nearbyClearSpawn(scene::Vec3 preferred,const Project& project,const Occupied& occupied){
    for(int i=0;i<=32;++i){
        const float angle=float((i-1)%8)*scene::kPi/4;
        const float radius=i?80.f*(1+(i-1)/8):0.f;
        const auto candidate=preferred+scene::Vec3{std::cos(angle)*radius,std::sin(angle)*radius,0};
        if(auto p=project(candidate);p&&!occupied(*p))return p;
    }
    return {};
}
template<class Project,class Occupied>
std::optional<scene::Vec3> validatedSpawn(scene::Vec3 preferred,const std::vector<scene::Vec3>& points,const std::vector<scene::Vec3>& fallbacks,const Project& project,const Occupied& occupied){
    if(auto p=nearbyClearSpawn(preferred,project,occupied))return p;
    for(auto point:points)if(auto p=project(point);p&&!occupied(*p))return p;
    for(auto point:fallbacks)if(auto p=nearbyClearSpawn(point,project,occupied))return p;
    return {};
}
template<class Occluded>
bool coveredSpawn(scene::Vec3 eye,scene::Vec3 feet,float height,scene::Vec2 halfExtents,const Occluded& occluded){
    const auto direction=scene::normalize(scene::Vec3{feet.x-eye.x,feet.y-eye.y,0});
    if(scene::length(direction)<.5f)return false;
    const scene::Vec3 side{-direction.y,direction.x,0};
    const float width=std::abs(side.x)*halfExtents.x+std::abs(side.y)*halfExtents.y;
    for(float fraction:{.08f,.5f,.97f})for(float lateral:{-1.f,0.f,1.f})
        if(!occluded(feet+scene::Vec3{0,0,height*fraction}+side*(width*lateral)))return false;
    return true;
}
inline bool courseOccludes(scene::Vec3 eye,scene::Vec3 target){
    const auto ray=target-eye;
    for(const auto& box:scene::course::kBoxes){
        const scene::Vec3 lo{box.minX,box.minY,0},hi{box.maxX,box.maxY,box.height};
        float enter=.001f,exit=.999f;
        const auto clip=[&](float start,float delta,float low,float high){
            if(std::abs(delta)<1e-5f)return start>=low&&start<=high;
            const float a=(low-start)/delta,b=(high-start)/delta;
            enter=std::max(enter,std::min(a,b));exit=std::min(exit,std::max(a,b));
            return enter<=exit;
        };
        if(clip(eye.x,ray.x,lo.x,hi.x)&&clip(eye.y,ray.y,lo.y,hi.y)&&clip(eye.z,ray.z,lo.z,hi.z))return true;
    }
    return false;
}
template<class Project,class Occupied,class Separation>
std::optional<scene::Vec3> chooseCoveredSpawn(const std::vector<scene::Vec3>& candidates,scene::Vec3 player,float scatter,
                                              const Project& project,const Occupied& occupied,const Separation& separation){
    const auto& ranked=candidates;
    const auto score=[&](scene::Vec3 p){
        const float distance=scene::length(p-player);
        return std::isfinite(distance)?(1.f-scatter)*distance+scatter*separation(p):-1.f;
    };
    std::vector<float> scores; scores.reserve(ranked.size());
    for(const auto p:ranked)scores.push_back(score(p));
    std::vector<std::size_t> order(ranked.size());
    for(std::size_t i=0;i<order.size();++i)order[i]=i;
    std::stable_sort(order.begin(),order.end(),[&](auto a,auto b){return scores[a]>scores[b];});
    for(const auto index:order)if(scores[index]>=0.f)
        if(auto p=project(ranked[index]);p&&!occupied(*p))return p;
    return {};
}
inline std::optional<scene::Vec3> chooseDistantSpawn(const std::vector<scene::Vec3>& source,scene::Vec3 player,SpawnHistory& history,std::size_t botCount){
    std::vector<scene::Vec3> points;
    for(auto p:source)if(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::none_of(points.begin(),points.end(),[&](auto q){return scene::length(p-q)<1.f;}))points.push_back(p);
    if(points.empty())return {};
    std::stable_sort(points.begin(),points.end(),[&](auto a,auto b){return scene::length(a-player)>scene::length(b-player);});
    const auto distant=static_cast<std::size_t>(std::count_if(points.begin(),points.end(),[&](auto p){return scene::length(p-player)>=10*iw::kMetersToUnits;}));
    if(distant>=3)points.resize(distant);
    points.resize(std::min(points.size(),std::max<std::size_t>(3,botCount)));
    auto pick=static_cast<std::size_t>(history.sequence++%points.size());
    if(points.size()>1&&history.last&&scene::length(points[pick]-*history.last)<1)pick=(pick+1)%points.size();
    history.last=points[pick];return points[pick];
}
}
