#include "render/RainField.h"
#include "render/ShelterCeilingReuse.h"
#include "render/RainBlockers.h"
#include "render/SurfaceWeather.h"
#include <sstream>
#include <iostream>
#include <limits>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<'\n';return 1;}}while(false)
int main(){
    using namespace render::rain;
    {ShelterCeilingReuse reuse;CHECK(!reuse.contains(5000));
     reuse.set(300,1000,true,500);CHECK(reuse.contains(310)&&reuse.contains(999));
     CHECK(!reuse.contains(309)&&!reuse.contains(1000));
     reuse.set(300,1000,true,305);CHECK(!reuse.contains(500)); // clipped projection
     reuse.set(300,1000,false,500);CHECK(!reuse.contains(500)); // empty/flat fallback
     reuse.set(-300,1000,true,-200);CHECK(reuse.contains(-290)&&!reuse.contains(-291));
     // Compare each allowed reuse against a fresh mesh selection/projection.
     for(float previous=0;previous<1200;previous+=7){
       float top=-1e8f,next=1e30f;int mask=0;
       const float lo[]={-100,400,800},hi[]={100,600,1000};
       for(int i=0;i<3;++i){if(lo[i]>previous)next=std::min(next,lo[i]);else{mask|=1<<i;top=std::max(top,hi[i]);}}
       reuse.set(top,next,true,previous);
       for(float current=0;current<1200;current+=11)if(reuse.contains(current)){
         float fresh=-1e8f;int freshMask=0;
         for(int i=0;i<3;++i)if(lo[i]<=current){freshMask|=1<<i;fresh=std::max(fresh,hi[i]);}
         CHECK(freshMask==mask);CHECK(std::min(fresh+10,current)==std::min(top+10,previous));
       }
     }
    }
    CHECK(nonblockingSurface("MP_Shipment_CN_Skybox_01"));CHECK(nonblockingSurface("invisible_wall"));CHECK(!nonblockingSurface("skyscraper_roof"));CHECK(!nonblockingSurface("roof"));
    CHECK(Settings{}.blockerDistance==50);
    {render::WetSettings s;CHECK(s.droplets==1&&s.dropSize==.23f&&s.rivulets==.277f&&s.flow==1);CHECK(s.detail.randomness==1.3f&&s.detail.flowDisplacement==.541f&&s.detail.flowDetail==7.721f&&s.detail.normalStrength==.342f);}
    {Settings defaults;CHECK(defaults.splashAmount==5&&defaults.splashSize==11.1f&&defaults.mistAmount==.49f&&defaults.mistSize==150);
    CHECK(defaults.style.rate==2.953f&&defaults.style.lifetime==.21f&&defaults.style.jitter==2&&defaults.style.fade==3.656f);
    CHECK(defaults.style.ringOpacity==.151f&&defaults.style.ringWidth==.001f&&defaults.style.grainScale==201.630f&&defaults.style.grainAmount==.955f);
    render::MuzzleLightSettings light;CHECK(light.radius==1000&&light.intensity==.07f&&light.falloff==.1f&&light.shape==1&&light.cone==270);}
    Settings s=preset(2),r;std::stringstream stream;stream<<s;stream>>r;
    CHECK(r.enabled&&r.density==1&&r.shelter&&r.splashAmount==1);
    std::stringstream broken("1 1 1");broken>>r;CHECK(broken.fail()&&r.density==1);
    r.radius=std::numeric_limits<float>::infinity();r.opacity=-10;r.quality=40;r.seed=-99;r.sanitize();CHECK(r.radius==18&&r.opacity==-10&&r.quality==2&&r.seed==-99);
    r.radius=100;r.speed=0;r.wind=-40;r.direction=900;r.sanitize();CHECK(r.radius==100&&r.wind==-40&&r.speed==0&&std::isfinite(slope(r).x));
    CHECK(phase(2.25,s)==phase(2.25,s));CHECK(std::isfinite(phase(-10000,s)));
    s.animate=false;s.freezeTime=2.25f;CHECK(phase(0,s)==phase(900,s));s.animate=true;
    scene::glb::Map map;
    const auto plane=[&](float size,float z){
        const scene::Vec3 p[4]={{-size,-size,z},{size,-size,z},{size,size,z},{-size,size,z}};
        for(auto t:{std::array<int,3>{0,1,2},std::array<int,3>{0,2,3}}){scene::glb::CollisionTriangle c;c.a=p[t[0]];c.b=p[t[1]];c.c=p[t[2]];c.normal={0,0,1};c.minimum={-size,-size,z};c.maximum={size,size,z};c.blocking=c.walkable=true;map.collision.push_back(c);}
    };
    plane(10000,0);plane(300,300);map.buildCollisionIndex();
    s.wind=0;Field f;CHECK(f.update(s,{0,0,100},&map));CHECK(f.queries==0);
    CHECK(f.lanes.size()==2401);for(const auto& lane:f.lanes)CHECK(lane.base.z==-1e8f);
    CHECK(!f.update(s,{0,0,100},&map)&&f.queries==0);
    const auto baseline=f.lanes;CHECK(f.update(s,{80,0,100},&map));CHECK(f.queries<f.lanes.size()/4);
    CHECK(f.update(s,{0,0,100},&map));CHECK(f.lanes.size()==baseline.size());
    for(std::size_t k=0;k<baseline.size();++k){CHECK(f.lanes[k].base.x==baseline[k].base.x&&f.lanes[k].base.z==baseline[k].base.z);}
    ++map.collisionRevision;CHECK(f.update(s,{0,0,100},&map));CHECK(f.queries==0);
    s.wind=4;CHECK(f.update(s,{0,0,100},&map));CHECK(f.queries==0);
    s.shelter=false;CHECK(f.update(s,{0,0,100},&map));CHECK(f.queries==0);for(const auto& l:f.lanes)CHECK(l.base.z== -1e8f);
    CHECK(f.update(s,{-10000,-10000,100},nullptr));CHECK(f.cache.size()==f.lanes.size());
    std::cout<<"PASS unrestricted settings, transactional presets, zero CPU collision queries, deterministic lanes, negative coordinates, map/wind invalidation\n";
}
