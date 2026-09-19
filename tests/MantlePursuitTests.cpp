#include "gameplay/MantleAcquisition.h"
#include "gameplay/BotSpawnPolicy.h"
#include "gameplay/BotRouteProbeCache.h"
#include <iostream>
#include <cstdlib>
static void check(bool value,const char* text){if(!value){std::cerr<<text<<'\n';std::exit(1);}}
static void quad(scene::glb::Map& m,scene::Vec3 a,scene::Vec3 b,scene::Vec3 c,scene::Vec3 d){
    for(auto vertices:{std::array{a,b,c},std::array{a,c,d}}){scene::glb::CollisionTriangle t;t.a=vertices[0];t.b=vertices[1];t.c=vertices[2];t.normal=scene::normalize(scene::cross(t.b-t.a,t.c-t.a));t.minimum=t.maximum=t.a;
        for(auto p:vertices){t.minimum={std::min(t.minimum.x,p.x),std::min(t.minimum.y,p.y),std::min(t.minimum.z,p.z)};t.maximum={std::max(t.maximum.x,p.x),std::max(t.maximum.y,p.y),std::max(t.maximum.z,p.z)};}
        t.walkable=t.normal.z>.7f;t.blocking=!t.walkable;m.collision.push_back(t);
    }
}
int main(){
    using namespace gameplay;scene::glb::Map map;
    bot::RouteProbeCache cache;int queries=0;bool blocked=false;
    const auto probe=[&](scene::Vec3,scene::Vec3 to)->std::optional<scene::Vec3>{++queries;return blocked?std::nullopt:std::optional{to};};
    cache.setRevision(1);check(cache.query({}, {100,0,0},probe).has_value(),"cache valid edge");
    cache.query({}, {100,0,0},probe);check(queries==1,"exact edge not reused");
    cache.query({}, {100,0,100},probe);check(queries==2,"different floor reused");
    blocked=true;cache.setRevision(2);check(!cache.query({}, {100,0,0},probe),"stale collision cache");
    cache.query({}, {100,0,0},probe);check(queries==3,"blocked edge not reused");
    quad(map,{-500,-500,0},{500,-500,0},{500,500,0},{-500,500,0});
    quad(map,{60,-200,100},{350,-200,100},{350,200,100},{60,200,100});
    quad(map,{60,200,0},{60,-200,0},{60,-200,100},{60,200,100});map.buildCollisionIndex();
    auto acquired=mantle::findClimb(map,{20,0,0},{1,0,0},38.1f,182.88f,45.72f,144.78f,106.68f,60.96f);
    check(acquired.has_value(),"clear ledge not acquired");
    const auto exact=mantle::findClimbTo(map,{20,0,0},{105,0,100},38.1f,182.88f,45.72f,144.78f,106.68f,60.96f);
    check(exact&&std::abs(exact->target.x-105)<.001f&&std::abs(exact->target.y)<.001f,"navigation mantle changed requested landing location");
    check(!mantle::findClimbTo(map,{20,0,0},{400,0,100},38.1f,182.88f,45.72f,144.78f,106.68f,60.96f),"navigation mantle accepted out-of-reach landing");
    auto previous=acquired->start;for(int i=1;i<=200;++i){auto next=acquired->sample(acquired->duration*i/200).position;auto hit=mantle::sweepRounded(map,previous,next,38.1f,182.88f);check(!hit.startSolid&&hit.fraction>=.9999f,"validated climb intersected collision during playback");previous=next;}
    check(scene::length(previous-acquired->target)<.001f,"climb endpoint drift");
    check(!mantle::findClimb(map,{20,0,0},{-1,0,0},38.1f,182.88f,45.72f,144.78f,106.68f,60.96f),"back-facing acquisition");
    auto tunnel=mantle::sweepRounded(map,{-200,0,0},{200,0,0},38.1f,182.88f);check(tunnel.fraction<1,"sweep tunneled through wall");
    quad(map,{60,-200,210},{60,200,210},{350,200,210},{350,-200,210});map.buildCollisionIndex();
    check(!mantle::findClimb(map,{20,0,0},{1,0,0},38.1f,182.88f,45.72f,144.78f,106.68f,60.96f),"low ceiling accepted");
    bot::SpawnHistory history;std::vector<scene::Vec3> spawns{{100,0,0},{600,0,0},{1200,0,0},{1500,0,0},{2000,0,0},{2000,0,0}};
    for(int i=0;i<100;++i){auto previous=history.last;auto p=bot::chooseDistantSpawn(spawns,{},history,5);check(p&&p->x>=1000,"spawn within ten metres despite three distant choices");check(!previous||scene::length(*p-*previous)>1,"consecutive duplicate spawn");}
    spawns={{100,0,0},{200,0,0},{300,0,0},{1200,0,0}};history={};bool closeFallback=false;
    for(int i=0;i<20;++i){auto previous=history.last;auto p=bot::chooseDistantSpawn(spawns,{},history,3);check(p&&p->x>=200,"fallback did not choose farthest set");closeFallback|=p->x<1000;check(!previous||scene::length(*p-*previous)>1,"fallback repeated spawn");}check(closeFallback,"fewer than three distant points did not use fallback");
    spawns={{100,0,0},{100,0,0}};check(bot::chooseDistantSpawn(spawns,{},history,5).has_value(),"single unique spawn unavailable");
    check(!bot::chooseDistantSpawn({}, {},history,5),"empty spawn pool produced invented spawn");
    std::cout<<"Mantle continuous path/ceiling/facing and distant nonrepeating spawn tests PASS\n";
}
