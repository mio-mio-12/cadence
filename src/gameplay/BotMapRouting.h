#pragma once

#include "gameplay/IwPresentation.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <queue>
#include <unordered_map>
#include <vector>
#include <array>

namespace gameplay::bot {

enum class MapRouteStatus { Idle, Searching, Following, Arrived, Unreachable };
struct MapRouteBudget {
    unsigned maxQueries{std::numeric_limits<unsigned>::max()};
    std::chrono::steady_clock::time_point deadline{std::chrono::steady_clock::time_point::max()};
    float maxDirectDistance{std::numeric_limits<float>::max()};
    bool followingPreview{}; // Offline planners need not move during search.
};
struct MapRouteResult {
    MapRouteStatus status{MapRouteStatus::Idle};
    std::optional<scene::Vec3> waypoint;
    unsigned expanded{};
};

// A bounded, incremental, collision-query graph. No synthetic test-course nodes,
// root-motion offsets, actor teleports, or full-map scans are involved.
struct BotMapRoute {
    struct Key {
        int x{}, y{}, z{};
        bool operator==(const Key&) const = default;
    };
    struct Hash {
        std::size_t operator()(Key k) const {
            return static_cast<std::uint32_t>(k.x)*73856093u ^
                   static_cast<std::uint32_t>(k.y)*19349663u ^
                   static_cast<std::uint32_t>(k.z)*83492791u;
        }
    };
    struct Node { scene::Vec3 point{}; float cost{}; std::size_t parent{}; bool closed{}; };
    struct Pending {
        float score{}, cost{}; std::size_t node{};
        bool operator<(const Pending& b) const {
            return score == b.score ? node > b.node : score > b.score;
        }
    };
    MapRouteStatus status{MapRouteStatus::Idle};
    scene::Vec3 origin{}, goal{}, lastPosition{}, directPosition{};
    float refreshTime{}, checkTime{}, noProgressTime{}, bestDistance{std::numeric_limits<float>::max()};
    float spacing{iw::worldUnits(32.0f)}, searchRadius{8000.0f};
    std::size_t nodeLimit{12000}, cursor{};
    unsigned searches{}, totalExpanded{};
    unsigned intentTag{};
    int preferredSide{};
    std::array<scene::Vec3,16> avoided{};
    std::size_t avoidedCount{};
    std::optional<std::size_t> activeNode;
    unsigned nextEdge{};
    bool goalPending{}, directPending{};
    std::vector<Node> nodes;
    std::unordered_map<Key,std::size_t,Hash> cells;
    std::priority_queue<Pending> open;
    std::vector<scene::Vec3> path;
    std::optional<scene::Vec3> preview;

    void reset() {
        status=MapRouteStatus::Idle; refreshTime=checkTime=noProgressTime=0;
        bestDistance=std::numeric_limits<float>::max();
        nodes.clear(); cells.clear(); open={}; path.clear(); cursor=0;
        activeNode.reset(); nextEdge=0; goalPending=directPending=false;
        preview.reset();
    }
};

inline float mapRouteDistance(scene::Vec3 a,scene::Vec3 b) { return scene::length(a-b); }

// Probe returns the grounded, capsule-clear endpoint of this edge or nullopt.
// Its contract includes walkable support, safe height changes and map boundaries;
// importantly it must not return the input height when no ground was found.
template<class Probe>
MapRouteResult updateMapRoute(BotMapRoute& route, scene::Vec3 position,
                             scene::Vec3 goal, float delta, const Probe& probe,
                             unsigned expansionBudget=24, MapRouteBudget budget={}) {
    unsigned queries=0;
    const auto canProbe=[&] {
        return expansionBudget>0 && queries<budget.maxQueries &&
            (budget.deadline==std::chrono::steady_clock::time_point::max() ||
             std::chrono::steady_clock::now()<budget.deadline);
    };
    const auto query=[&](scene::Vec3 from,scene::Vec3 to) {
        ++queries;return probe(from,to);
    };
    delta=std::max(0.0f,delta);
    route.refreshTime=std::max(0.0f,route.refreshTime-delta);
    route.checkTime=std::max(0.0f,route.checkTime-delta);
    const float spacing=std::max(8.0f,route.spacing);
    const auto key=[&](scene::Vec3 p) {
        return BotMapRoute::Key{static_cast<int>(std::lround((p.x-route.origin.x)/spacing)),
            static_cast<int>(std::lround((p.y-route.origin.y)/spacing)),
            static_cast<int>(std::lround((p.z-route.origin.z)/std::max(8.0f,spacing*.5f)))};
    };
    const auto finish=[&](MapRouteStatus status) {
        route.status=status;
        route.refreshTime=status==MapRouteStatus::Unreachable?3.0f:.75f;
        route.nodes.clear();route.cells.clear();route.open={};
        route.activeNode.reset();route.goalPending=route.directPending=false;
        route.preview.reset();
        if(status==MapRouteStatus::Following&&!route.path.empty()){
            // An anytime prefix may already have carried the bot along this
            // path. Do not send it back to the original search origin.
            std::size_t nearest=0;float best=mapRouteDistance(position,route.path[0]);
            for(std::size_t i=1;i<route.path.size();++i){float d=mapRouteDistance(position,route.path[i]);if(d<best){best=d;nearest=i;}}
            route.cursor=nearest;
        }
    };
    const bool movedGoal=mapRouteDistance(goal,route.goal)>spacing*3.0f;
    const bool teleported=route.status!=MapRouteStatus::Idle &&
        mapRouteDistance(position,route.lastPosition)>iw::worldUnits(128.0f);
    if(teleported)route.reset();
    // Momentum or reaction travel may carry an actor beyond its arrival radius.
    if(route.status==MapRouteStatus::Arrived&&mapRouteDistance(position,goal)>std::min(45.f,spacing*1.1f))route.reset();
    route.lastPosition=position;
    if(route.status==MapRouteStatus::Idle ||
       (route.status!=MapRouteStatus::Searching && route.refreshTime<=0 &&
        (movedGoal || route.status==MapRouteStatus::Unreachable))) {
        route.reset();route.origin=position;route.goal=goal;route.lastPosition=position;
        ++route.searches;
        if(mapRouteDistance(position,goal)>route.searchRadius ||
           !std::isfinite(goal.x)||!std::isfinite(goal.y)||!std::isfinite(goal.z)) {
            finish(MapRouteStatus::Unreachable);
        } else {
            route.status=MapRouteStatus::Searching;
            route.goalPending=true;
            // Validate a direct route in resumable short sweeps. A long open
            // corridor should not require a full lattice search.
            route.directPending=true;route.directPosition=position;
            route.nodes.push_back({position,0,0,false});
            route.cells.emplace(key(position),0);
            route.open.push({mapRouteDistance(position,goal),0,0});
        }
    }
    unsigned expanded=0;
    if(route.status==MapRouteStatus::Searching && route.goalPending && canProbe()) {
        route.goalPending=false;
        const auto endpoint=query(route.goal,route.goal);
        if(!endpoint || mapRouteDistance(*endpoint,route.goal)>=spacing*.6f)
            finish(MapRouteStatus::Unreachable);
    }
    while(route.status==MapRouteStatus::Searching && !route.goalPending && route.directPending && canProbe()) {
        const float distance=mapRouteDistance(route.directPosition,route.goal);
        const float limit=std::max(8.0f,budget.maxDirectDistance);
        const bool last=distance<=limit;
        const auto desired=last?route.goal:
            route.directPosition+(route.goal-route.directPosition)*(limit/distance);
        const auto endpoint=query(route.directPosition,desired);
        if(!endpoint || (last && mapRouteDistance(*endpoint,route.goal)>=spacing*.6f) ||
           (!last && mapRouteDistance(*endpoint,route.goal)>=distance-.01f)) {
            // None of a partially validated shortcut is a completed route.
            route.path.clear();route.directPending=false;
        } else {
            route.path.push_back(*endpoint);route.directPosition=*endpoint;
            if(last)finish(MapRouteStatus::Following);
        }
    }
    if(route.preview&&mapRouteDistance(position,*route.preview)<spacing*.6f){route.preview.reset();route.bestDistance=std::numeric_limits<float>::max();route.noProgressTime=0;}
    if(budget.followingPreview&&route.status==MapRouteStatus::Searching&&route.preview){
        const float distance=mapRouteDistance(position,*route.preview);
        if(distance<route.bestDistance-2){route.bestDistance=distance;route.noProgressTime=0;}else route.noProgressTime+=delta;
        if(route.noProgressTime>1.5f){route.reset();return {MapRouteStatus::Searching,{},0};}
    }
    if(route.status==MapRouteStatus::Searching&&!route.goalPending&&!route.directPending&&!route.preview&&!route.open.empty()&&canProbe()){
        std::vector<scene::Vec3> prefix;
        for(auto node=route.open.top().node;node!=0;node=route.nodes[node].parent)prefix.push_back(route.nodes[node].point);
        std::reverse(prefix.begin(),prefix.end());
        if(!prefix.empty()){
            std::size_t nearest=0;float best=mapRouteDistance(position,prefix[0]);
            for(std::size_t i=1;i<prefix.size();++i){float d=mapRouteDistance(position,prefix[i]);if(d<best){best=d;nearest=i;}}
            if(best<spacing*.6f&&nearest+1<prefix.size())++nearest;
            // Do not climb onto a one-way sill while the search still has no
            // validated exit. Ground-level prefixes are safe to follow early.
            if(std::abs(prefix[nearest].z-position.z)<=iw::worldUnits(18.f)&&mapRouteDistance(position,prefix[nearest])<=budget.maxDirectDistance){
                if(auto valid=query(position,prefix[nearest]);valid&&mapRouteDistance(*valid,prefix[nearest])<spacing*.6f)route.preview=prefix[nearest];
            }
        }
    }
    while(route.status==MapRouteStatus::Searching && !route.goalPending && !route.directPending && canProbe()) {
        if(!route.activeNode) {
            if(expanded>=expansionBudget)break;
            if(route.open.empty() || route.nodes.size()>=route.nodeLimit) {
                finish(MapRouteStatus::Unreachable);break;
            }
            const auto pending=route.open.top();route.open.pop();
            if(route.nodes[pending.node].closed || pending.cost>route.nodes[pending.node].cost+.001f)continue;
            route.nodes[pending.node].closed=true;
            route.activeNode=pending.node;route.nextEdge=0;
            ++expanded;++route.totalExpanded;
        }
        const auto active=*route.activeNode;
        const auto current=route.nodes[active];
        if(route.nextEdge==0) {
            if(mapRouteDistance(current.point,route.goal)<=spacing*1.5f) {
                if(auto end=query(current.point,route.goal);
                   end && mapRouteDistance(*end,route.goal)<spacing*.6f) {
                    route.path.push_back(*end);
                    for(auto index=active;index!=0;index=route.nodes[index].parent)
                        route.path.push_back(route.nodes[index].point);
                    std::reverse(route.path.begin(),route.path.end());
                    finish(MapRouteStatus::Following);break;
                }
            }
            route.nextEdge=1;
            // Once around a corner, try a resumable straight corridor to the
            // goal instead of expanding every floor cell along that corridor.
            // Reuse the same chunk/query deadline as the initial shortcut.
            if(route.totalExpanded%16==1&&mapRouteDistance(current.point,route.goal)>spacing*1.5f){
                route.path.clear();
                for(auto index=active;index!=0;index=route.nodes[index].parent)route.path.push_back(route.nodes[index].point);
                std::reverse(route.path.begin(),route.path.end());
                route.directPosition=current.point;route.directPending=true;break;
            }
        }
        // Persist the exact next edge. A budget yield is never a blocked edge.
        while(route.nextEdge<=18) {
            if(!canProbe())break;
            const unsigned ordinal=route.nextEdge++-1,edge=ordinal%9;
            const int x=static_cast<int>(edge%3)-1,y=static_cast<int>(edge/3)-1;
            if(x==0&&y==0)continue;
            const float stride=ordinal<9?3.f:1.f;
            // Keep the lattice anchored to this search's start, like key().
            // Snapping to the world's unrelated grid shifts the first edge
            // sideways into nearby walls even when a parallel lane is clear.
            const scene::Vec3 desired{route.origin.x+(std::round((current.point.x-route.origin.x)/spacing)+x*stride)*spacing,route.origin.y+(std::round((current.point.y-route.origin.y)/spacing)+y*stride)*spacing,current.point.z};
            if(mapRouteDistance(desired,route.origin)>route.searchRadius)continue;
            const auto endpoint=query(current.point,desired);
            if(!endpoint)continue;
            const auto cell=key(*endpoint);
            const float length=mapRouteDistance(current.point,*endpoint);
            float penalty=0;
            for(std::size_t i=0;i<route.avoidedCount;++i)penalty=std::max(penalty,std::max(0.f,1-mapRouteDistance(*endpoint,route.avoided[i])/180.f)*.65f);
            if(route.preferredSide){const auto line=route.goal-route.origin,offset=*endpoint-route.origin;const float side=line.x*offset.y-line.y*offset.x;if(side*route.preferredSide<0)penalty+=.10f;}
            const float cost=current.cost+length*(1+penalty);
            const auto found=route.cells.find(cell);
            std::size_t index;
            if(found==route.cells.end()) {
                index=route.nodes.size();
                route.nodes.push_back({*endpoint,cost,active,false});
                route.cells.emplace(cell,index);
            } else {
                index=found->second;
                if(route.nodes[index].closed||cost>=route.nodes[index].cost-.001f)continue;
                route.nodes[index].cost=cost;route.nodes[index].parent=active;
            }
            route.open.push({cost+mapRouteDistance(*endpoint,route.goal)*2.f,cost,index});
        }
        if(route.nextEdge>18)route.activeNode.reset();
    }
    if(route.status==MapRouteStatus::Following) {
        const auto reaches=[&](scene::Vec3 target){const auto end=query(position,target);return end&&mapRouteDistance(*end,target)<std::min(20.f,spacing*.3f);};
        while(route.cursor<route.path.size() &&
              mapRouteDistance(position,route.path[route.cursor])<std::min(26.f,spacing*.65f)) {
            if(route.cursor+1<route.path.size() &&
               (!canProbe() || !reaches(route.path[route.cursor+1])))break;
            ++route.cursor;route.noProgressTime=0;
            route.bestDistance=std::numeric_limits<float>::max();
        }
        if(route.cursor==route.path.size())finish(MapRouteStatus::Arrived);
        else {
            // Short, bounded lookahead smooths lattice corners without cutting walls.
            if(route.checkTime<=0 && canProbe()) {
                route.checkTime=.15f;
                if(!reaches(route.path[route.cursor])) {
                    route.reset();return {MapRouteStatus::Searching,{},expanded};
                }
                const auto last=std::min(route.path.size()-1,route.cursor+6);
                for(auto next=last;next>route.cursor && canProbe();--next)if(
                    mapRouteDistance(position,route.path[next])<=std::max(8.0f,budget.maxDirectDistance) &&
                    reaches(route.path[next])) {
                    route.cursor=next;route.noProgressTime=0;
                    route.bestDistance=std::numeric_limits<float>::max();break;
                }
            }
            const float distance=mapRouteDistance(position,route.path[route.cursor]);
            if(distance<route.bestDistance-2.0f) {
                route.bestDistance=distance;route.noProgressTime=0;
            } else route.noProgressTime+=delta;
            // Crowd/physics blockage requests a fresh route; never a random shove.
            if(route.noProgressTime>1.5f) {
                route.reset();return {MapRouteStatus::Searching,{},expanded};
            }
            return {route.status,route.path[route.cursor],expanded};
        }
    }
    return {route.status,route.status==MapRouteStatus::Searching?route.preview:std::nullopt,expanded};
}

} // namespace gameplay::bot
