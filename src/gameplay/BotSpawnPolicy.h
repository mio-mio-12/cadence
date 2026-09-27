#pragma once
#include "gameplay/IwPresentation.h"
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
