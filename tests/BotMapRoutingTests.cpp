#include "gameplay/BotMapRouting.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>

using namespace gameplay::bot;
struct Box {float x0,y0,x1,y1;};
struct World {
    std::vector<Box> walls;
    bool ledge{};
    mutable unsigned calls{};
    std::optional<scene::Vec3> operator()(scene::Vec3 from,scene::Vec3 to)const {
        ++calls;
        if(std::abs(to.z-from.z)>45 || (ledge&&to.x>100))return {};
        // Model an already capsule-expanded wall and walkable floor.
        for(const auto& b:walls) {
            float lo=0,hi=1;
            const auto slab=[&](float start,float end,float min,float max) {
                if(std::abs(end-start)<.00001f)return start>min&&start<max;
                float a=(min-start)/(end-start),z=(max-start)/(end-start);
                if(a>z)std::swap(a,z);lo=std::max(lo,a);hi=std::min(hi,z);return lo<hi;
            };
            if(slab(from.x,to.x,b.x0,b.x1)&&slab(from.y,to.y,b.y0,b.y1))return {};
        }
        to.z=0;return to;
    }
};
static MapRouteResult search(BotMapRoute& r,scene::Vec3 p,scene::Vec3 goal,const World& w) {
    MapRouteResult result;
    for(int i=0;i<1500;++i) {
        const auto before=r.totalExpanded;
        result=updateMapRoute(r,p,goal,.008f,w,24);
        assert(r.totalExpanded-before<=24);
        if(result.status!=MapRouteStatus::Searching)return result;
    }
    assert(false&&"bounded search completion");return result;
}
static void follow(BotMapRoute& r,scene::Vec3 p,scene::Vec3 goal,const World& w) {
    for(int i=0;i<20000;++i) {
        auto result=updateMapRoute(r,p,goal,.008f,w,24);
        if(result.status==MapRouteStatus::Arrived) {
            assert(scene::length(p-goal)<r.spacing);return;
        }
        assert(result.status!=MapRouteStatus::Unreachable);
        if(result.waypoint) {
            auto d=*result.waypoint-p;
            const auto next=p+d*(std::min(4.0f,scene::length(d))/std::max(.001f,scene::length(d)));
            if(!w(p,next))std::cerr<<"Crossing "<<p.x<<","<<p.y<<" -> "<<next.x<<","<<next.y<<" waypoint "<<result.waypoint->x<<","<<result.waypoint->y<<"\n";
            assert(w(p,next));p=next;
        }
    }
    assert(false&&"route reaches goal without wall crossings");
}
int main() {
    {
        BotMapRoute r;World w;w.ledge=true;
        MapRouteBudget budget;budget.maxQueries=1;
        assert(updateMapRoute(r,{0,0,0},{400,0,0},.008f,w,0,budget).status==MapRouteStatus::Searching);
        assert(w.calls==0 && r.goalPending);
        assert(updateMapRoute(r,{0,0,0},{400,0,0},.008f,w,24,budget).status==MapRouteStatus::Unreachable);
        assert(w.calls==1 && r.totalExpanded==0);
        updateMapRoute(r,{0,0,0},{400,0,0},.008f,w,24,budget);
        assert(w.calls==1);
    }
    {
        // Pausing before the initial query must not turn an unknown edge into
        // an obstruction or consume the pending shortcut.
        BotMapRoute r;World w;
        MapRouteBudget budget;budget.maxQueries=0;
        assert(updateMapRoute(r,{0,0,0},{400,0,0},.008f,w,24,budget).status==MapRouteStatus::Searching);
        assert(w.calls==0 && r.directPending);
        budget.maxQueries=1;budget.deadline=std::chrono::steady_clock::now();
        updateMapRoute(r,{0,0,0},{400,0,0},.008f,w,24,budget);
        assert(w.calls==0 && r.directPending);
        budget.deadline=std::chrono::steady_clock::time_point::max();
        assert(updateMapRoute(r,{0,0,0},{400,0,0},.008f,w,24,budget).status==MapRouteStatus::Searching);
        assert(w.calls==1 && !r.goalPending && r.directPending);
        assert(updateMapRoute(r,{0,0,0},{400,0,0},.008f,w,24,budget).waypoint);
        assert(w.calls==2);
    }
    {
        // Interleave independent bots with a one-query allowance. Every route
        // must continue through all eight neighbors and find the same detour.
        BotMapRoute routes[3];World w{{{100,-600,200,300},{100,200,600,300}}};
        BotMapRoute reference;
        assert(search(reference,{0,0,0},{500,0,0},w).waypoint);
        MapRouteBudget budget;budget.maxQueries=1;budget.maxDirectDistance=600;
        for(int tick=0;tick<20000;++tick) {
            bool done=true;
            for(auto& r:routes) {
                if(r.status==MapRouteStatus::Following)continue;
                done=false;
                const auto calls=w.calls;
                updateMapRoute(r,{0,0,0},{500,0,0},.008f,w,24,budget);
                assert(w.calls-calls<=1);
                assert(r.status!=MapRouteStatus::Unreachable);
            }
            if(done)break;
        }
        for(auto& r:routes) {
            assert(r.status==MapRouteStatus::Following);
            assert(r.totalExpanded==reference.totalExpanded);
            follow(r,{0,0,0},{500,0,0},w);
        }
    }
    {
        BotMapRoute r;World w;
        MapRouteBudget budget;budget.maxQueries=1;budget.maxDirectDistance=500;
        float longest=0;
        const auto probe=[&](scene::Vec3 from,scene::Vec3 to) {
            longest=std::max(longest,mapRouteDistance(from,to));return w(from,to);
        };
        for(int i=0;i<9 && r.status!=MapRouteStatus::Following;++i)
            updateMapRoute(r,{0,0,0},{4000,0,0},.008f,probe,24,budget);
        assert(r.status==MapRouteStatus::Following);
        assert(r.totalExpanded==0 && w.calls==9 && r.path.size()==8);
        assert(longest<=500); // Eight resumable sweeps, no lattice expansion.
        const auto calls=w.calls;
        assert(updateMapRoute(r,{0,0,0},{4000,0,0},.008f,w,0).waypoint);
        assert(w.calls==calls);
    }
    {
        // A wall in a later shortcut chunk must discard the partial shortcut
        // and still find a collision-safe detour from the original position.
        BotMapRoute r;World w{{{600,-150,700,150}}};
        MapRouteBudget budget;budget.maxQueries=1;budget.maxDirectDistance=500;
        for(int i=0;i<10000 && r.status!=MapRouteStatus::Following;++i) {
            const auto calls=w.calls;
            updateMapRoute(r,{0,0,0},{1000,0,0},.008f,w,24,budget);
            assert(w.calls-calls<=1);
        }
        assert(r.status==MapRouteStatus::Following && r.totalExpanded>0);
        follow(r,{0,0,0},{1000,0,0},w);
    }
    {
        // Each continuation must start at the previous grounded endpoint,
        // even when ground differs from the interpolated shortcut height.
        BotMapRoute r;MapRouteBudget budget;budget.maxQueries=1;budget.maxDirectDistance=500;
        const auto ground=[](float x){return x*x*.00001f;};
        const auto probe=[&](scene::Vec3 from,scene::Vec3 to)->std::optional<scene::Vec3> {
            assert(std::abs(from.z-ground(from.x))<.01f);
            assert(mapRouteDistance(from,to)<=500.001f);
            to.z=ground(to.x);return to;
        };
        for(int i=0;i<12 && r.status!=MapRouteStatus::Following;++i)
            updateMapRoute(r,{0,0,0},{4000,0,ground(4000)},.008f,probe,24,budget);
        assert(r.status==MapRouteStatus::Following && r.totalExpanded==0);
    }
    {
        BotMapRoute r;World w;
        auto result=search(r,{0,0,0},{1000,0,0},w);
        assert(result.waypoint&&r.totalExpanded==0);
        follow(r,{0,0,0},{1000,0,0},w);
    }
    {
        BotMapRoute r;World w{{{100,-1900,200,1900}}};
        auto result=search(r,{0,0,0},{400,0,0},w);
        assert(result.waypoint);assert(r.searches==1);
        bool aroundEnd=false;for(const auto p:r.path)aroundEnd|=std::abs(p.y)>1900;
        assert(aroundEnd);follow(r,{0,0,0},{400,0,0},w);
        std::cout<<"38 m wall detour: "<<r.totalExpanded<<" nodes\n";
    }
    {
        // L-corner route; immediate obstruction and turn through a safe opening.
        BotMapRoute r;World w{{{100,-600,200,300},{100,200,600,300}}};
        assert(search(r,{0,0,0},{500,0,0},w).waypoint);
        follow(r,{0,0,0},{500,0,0},w);
    }
    {
        BotMapRoute r;World w{{{100,-6000,200,70},{100,130,200,6000}}};
        assert(search(r,{0,0,0},{400,0,0},w).waypoint);
        follow(r,{0,0,0},{400,0,0},w);
    }
    {
        BotMapRoute r;r.nodeLimit=120;World w{{{100,-6000,200,6000}}};
        assert(search(r,{0,0,0},{400,0,0},w).status==MapRouteStatus::Unreachable);
        const auto expansions=r.totalExpanded,searches=r.searches,calls=w.calls;
        for(int i=0;i<100;++i)assert(updateMapRoute(r,{0,0,0},{400,0,0},.008f,w).status==MapRouteStatus::Unreachable);
        assert(r.searches==searches&&r.totalExpanded==expansions&&w.calls==calls);
    }
    {
        BotMapRoute r;r.nodeLimit=100;World w;
        assert(search(r,{0,0,0},{0,0,300},w).status==MapRouteStatus::Unreachable);
        assert(r.path.empty());
    }
    {
        BotMapRoute r;World w;
        assert(search(r,{0,0,0},{9000,0,0},w).status==MapRouteStatus::Unreachable);
        assert(r.totalExpanded==0);
    }
    {
        BotMapRoute r;r.nodeLimit=100;World w;w.ledge=true;
        assert(search(r,{0,0,0},{400,0,0},w).status==MapRouteStatus::Unreachable);
    }
    {
        BotMapRoute r;World w;
        assert(search(r,{0,0,0},{400,0,0},w).waypoint);
        const auto searches=r.searches;
        assert(updateMapRoute(r,{800,800,0},{400,0,0},.008f,w).waypoint);
        assert(r.searches==searches+1);
    }
    {
        BotMapRoute r;World w{{{100,-600,200,300},{100,200,600,300}}};
        MapRouteBudget budget;budget.maxQueries=1;budget.maxDirectDistance=500;budget.followingPreview=true;
        for(int i=0;i<2000&&!r.preview;++i)updateMapRoute(r,{0,0,0},{500,0,0},.008f,w,24,budget);
        assert(r.status==MapRouteStatus::Searching&&r.preview);
        const auto calls=w.calls;budget.maxQueries=0;
        for(int i=0;i<200&&r.status!=MapRouteStatus::Idle;++i)updateMapRoute(r,{0,0,0},{500,0,0},.008f,w,24,budget);
        assert(r.status==MapRouteStatus::Idle&&!r.preview&&w.calls==calls);
    }
    {
        BotMapRoute r;World w;r.status=MapRouteStatus::Arrived;r.goal={500,0,0};r.lastPosition={560,0,0};
        assert(updateMapRoute(r,{560,0,0},{500,0,0},.008f,w).waypoint);
    }
    {
        // An incomplete search must not entice the body up onto a ledge whose
        // way out has not been found. Ground prefixes still advance normally.
        BotMapRoute r;r.status=MapRouteStatus::Searching;r.origin=r.lastPosition={0,0,0};r.goal={500,0,0};
        r.nodes={{{0,0,0},0,0,true},{{81,0,100},130,0,false}};
        r.open.push({200,130,1});
        MapRouteBudget budget;budget.maxQueries=1;budget.maxDirectDistance=500;budget.followingPreview=true;
        const auto probe=[](scene::Vec3,scene::Vec3 p)->std::optional<scene::Vec3>{return p;};
        const auto result=updateMapRoute(r,{0,0,0},{500,0,0},.008f,probe,1,budget);
        assert(!r.preview && (!result.waypoint||result.status==MapRouteStatus::Following));
    }
    {
        // A probe may offer a different mantle endpoint. It cannot validate
        // this path's requested point, particularly on an overlapping floor.
        BotMapRoute r;r.status=MapRouteStatus::Following;r.goal={500,0,0};r.path={{100,0,0},{500,0,0}};
        const auto wrongFloor=[](scene::Vec3,scene::Vec3 p)->std::optional<scene::Vec3>{p.z+=100;return p;};
        const auto result=updateMapRoute(r,{0,0,0},{500,0,0},.008f,wrongFloor);
        assert(!result.waypoint&&r.status==MapRouteStatus::Idle);
    }
    {
        // A narrow L lane offset from world zero must retain its centerline.
        BotMapRoute r;r.spacing=80;r.searchRadius=1000;
        const auto lane=[](scene::Vec3 a,scene::Vec3 b)->std::optional<scene::Vec3>{
            for(int i=0;i<=40;++i){auto p=scene::lerp(a,b,i/40.f);
                if(!(std::abs(p.x-37)<8&&p.y>=20&&p.y<=340)&&
                   !(std::abs(p.y-333)<8&&p.x>=29&&p.x<=445))return {};
            }return b;
        };
        for(int i=0;i<100&&r.status!=MapRouteStatus::Following;++i)
            updateMapRoute(r,{37,93,0},{437,333,0},.008f,lane,24);
        assert(r.status==MapRouteStatus::Following);
    }
    std::cout<<"Bot map routing tests passed\n";
}
