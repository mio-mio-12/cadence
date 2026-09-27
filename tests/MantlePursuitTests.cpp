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
    {
        gameplay::mantle::CameraSmoothing c;c.begin(0,0);
        const float first=c.update(20,.008f,true);
        check(first>0&&first<2,"mantle camera snaps to first target");
        float prior=first;
        for(int i=0;i<120;++i){const float z=c.update(20,.008f,false);check(z>=prior-.001f&&z<=20.001f,"mantle camera overshoot");prior=z;}
        check(std::abs(prior-20)<.01f&&c.remaining==0,"mantle camera tail did not settle");
    }
    {
        gameplay::mantle::Presentation p;
        check(!p.active(0), "unstarted mantle presentation active");
        p.begin(10,.12f,32,1);
        check(p.duration>.5f&&p.active(10.13), "quick traversal rushed presentation");
        check(p.phase(10.12)<.25f, "presentation jumped to final frame at physical exit");
        check(!p.active(11)&&p.phase(11)==1, "presentation never completed");
        p.begin(20,.8f,32,1);
        check(p.duration==.8f, "long traversal presentation shortened");
        p.begin(30,.06f,32,2);
        check(p.duration>.25f&&p.duration<.3f, "explicit mantle speed not respected");
        p.cancel();check(!p.active(30.1), "blocked mantle presentation continued");
    }
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
    {
        scene::glb::Map::MantleGround shared;
        for(float z:{0.f,3.9f,4.1f,25.f,120.f,300.f})for(float r:{20.f,38.1f})for(float step:{30.f,45.72f})for(int i=0;i<16;++i){
            const scene::Vec3 p{-40,0,z},f{std::cos(i*scene::kPi/8),std::sin(i*scene::kPi/8),0};
            const auto plain=map.mantleTarget(p,f,r,182.88f,step,144.78f,150,10);
            const auto reused=map.mantleTarget(p,f,r,182.88f,step,144.78f,150,10,true,&shared);
            check(bool(plain)==bool(reused)&&(!plain||scene::length(*plain-*reused)==0),"shared mantle ground changed target");
        }
        scene::glb::Map empty;
        check(!empty.mantleTarget({0,0,25},{1,0,0},38.1f,182.88f,45.72f,144.78f,150,10,true,&shared),"mantle ground reused across maps");
    }
    check(acquired.has_value(),"clear ledge not acquired");
    {
        const scene::Vec3 start{60-38.1f-.02f,0,0},end{start.x,start.y,150};
        const auto full=mantle::sweepRounded(map,start,end,38.1f,182.88f);
        check(!full.startSolid&&full.fraction>=.9999f,"parallel ascent catches an inflated ledge shell");
        auto previous=start;
        for(int i=1;i<=300;++i){const auto next=scene::lerp(start,end,i/300.f);const auto hit=mantle::sweepRounded(map,previous,next,38.1f,182.88f);check(!hit.startSolid&&hit.fraction>=.9999f,"short ascent snags rounded ledge edge");previous=next;}
        check(mantle::sweepRounded(map,start,start+scene::Vec3{5,0,0},38.1f,182.88f).fraction<1,"contact tolerance allowed movement into wall");
    }
    {
        mantle::ApproachMomentum memory;memory.contact({800,0,150},{0,0,140},true);
        check(memory.entry({0,0,140},{1,0,0}).x==800,"impact lost approach velocity");
        check(memory.entry({0,0,140},{-1,0,0}).x==0,"turning away resurrected momentum");
        memory.age(.1f,true);check(memory.entry({0,0,140},{1,0,0}).x==0,"stale impact momentum");
        for(bool boxHull:{false,true})for(float speed:{600.f,900.f,1600.f})for(float vertical:{0.f,150.f,-100.f}){
            const scene::Vec3 start{-70,0,25},velocity{speed,0,vertical};
            auto fallback=mantle::findClimb(map,start,{1,0,0},38.1f,182.88f,45.72f,144.78f,230,60.96f,1,boxHull);
            check(fallback.has_value(),"fast approach missing supported ledge");
            const auto sweep=[&](scene::Vec3 a,scene::Vec3 b){return boxHull?movement::sweep(map,a,b,38.1f,182.88f):mantle::sweepRounded(map,a,b,38.1f,182.88f);};
            const auto fast=mantle::withMomentum(*fallback,velocity,{1,0,0},1,sweep);
            check(!fast.liftFirst,"fast clear approach fell back to stationary lift");
            check(fast.sample(.004f).position.x>start.x+speed*.003f,"fast mantle entry pauses horizontally");
            check(std::abs(fast.sample(0).velocity.z-vertical)<.01f,"airborne vertical velocity lost");
            check(std::abs(fast.sample(fast.duration).velocity.x-speed)<.01f,"fast mantle exit loses speed");
            check(mantle::clear(fast,sweep),"fast mantle crosses collision");
        }
        const auto rejected=mantle::withMomentum(*acquired,{800,0,0},{1,0,0},1,[](scene::Vec3 a,scene::Vec3){movement::Trace h;h.end=a;h.fraction=0;return h;});
        check(rejected.liftFirst,"blocked momentum path bypassed safe fallback");
    }
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
    {
        const auto project=[](scene::Vec3 p)->std::optional<scene::Vec3>{if(p.x<0)return {};p.z=4;return p;};
        const auto occupied=[](scene::Vec3 p){return scene::length(p-scene::Vec3{0,0,4})<100;};
        const auto clear=bot::nearbyClearSpawn({},project,occupied);
        check(clear&&clear->x>=0&&clear->z==4&&!occupied(*clear),"spawn overlaps prop or another bot");
        check(!bot::nearbyClearSpawn({},[](scene::Vec3)->std::optional<scene::Vec3>{return {};},occupied),"blocked spawn invented a valid position");
    }
    for(int i=0;i<100;++i){auto previous=history.last;auto p=bot::chooseDistantSpawn(spawns,{},history,5);check(p&&p->x>=1000,"spawn within ten metres despite three distant choices");check(!previous||scene::length(*p-*previous)>1,"consecutive duplicate spawn");}
    spawns={{100,0,0},{200,0,0},{300,0,0},{1200,0,0}};history={};bool closeFallback=false;
    for(int i=0;i<20;++i){auto previous=history.last;auto p=bot::chooseDistantSpawn(spawns,{},history,3);check(p&&p->x>=200,"fallback did not choose farthest set");closeFallback|=p->x<1000;check(!previous||scene::length(*p-*previous)>1,"fallback repeated spawn");}check(closeFallback,"fewer than three distant points did not use fallback");
    spawns={{100,0,0},{100,0,0}};check(bot::chooseDistantSpawn(spawns,{},history,5).has_value(),"single unique spawn unavailable");
    check(!bot::chooseDistantSpawn({}, {},history,5),"empty spawn pool produced invented spawn");
    std::cout<<"Mantle continuous path/ceiling/facing and distant nonrepeating spawn tests PASS\n";
}
