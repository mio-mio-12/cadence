#include "gameplay/CssMovement.h"
#include "SourceSdkMovementOracle.h"
#include <iostream>
using namespace gameplay::css;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<'\n';return 1;}}while(false)
bool near(float a,float b,float e=.001f){return std::abs(a-b)<e;}
auto empty=[](Vec3,Vec3 b,float=16,Vec3=Vec3{}){return Trace{1,b,{},false};};
auto floorTrace=[](Vec3 a,Vec3 b,float=16,Vec3=Vec3{}){Trace t{1,b,{},false};if(b.z<0&&a.z>=0){t.fraction=a.z/(a.z-b.z);t.end=scene::lerp(a,b,t.fraction);t.normal={0,0,1};}return t;};
void quad(scene::glb::Map& map,Vec3 a,Vec3 b,Vec3 c,Vec3 d){
    for(auto v:{std::array{a,b,c},std::array{a,c,d}}){scene::glb::CollisionTriangle t;t.a=v[0];t.b=v[1];t.c=v[2];t.normal=scene::normalize(scene::cross(t.b-t.a,t.c-t.a));t.minimum=t.maximum=t.a;
        for(auto p:v){t.minimum={std::min(t.minimum.x,p.x),std::min(t.minimum.y,p.y),std::min(t.minimum.z,p.z)};t.maximum={std::max(t.maximum.x,p.x),std::max(t.maximum.y,p.y),std::max(t.maximum.z,p.z)};}
        t.walkable=t.normal.z>=.7f;t.blocking=!t.walkable;map.collision.push_back(t);
    }
}
int main(){
    // Differential check against the actual, unmodified SDK function bodies,
    // not a second copy of the Cadence implementation or symmetry alone.
    for(bool airMode:{false,true})for(float surface:{.25f,1.f})for(float accel:{5.f,10.f,150.f})for(int i=0;i<500;++i){
        const float angle=i*.017f,speed=50.f+float(i%251);
        Vec3 v{float(i%127-63)*21.f,float(i%113-56)*17.f,float(i%31-15)*13.f};
        const auto desired=Vec3{std::cos(angle),std::sin(angle),0}*speed;
        const float actualSpeed=scene::length(desired);const auto direction=desired/actualSpeed;
        source_sdk_oracle::Player p;p.m_surfaceFriction=surface;
        source_sdk_oracle::Move m;m.m_vecVelocity={{v.x,v.y,v.z}};
        source_sdk_oracle::CGameMovement sdk{&p,&m};source_sdk_oracle::Vector d{{direction.x,direction.y,direction.z}};
        if(airMode)sdk.AirAccelerate(d,actualSpeed,accel);else sdk.Accelerate(d,actualSpeed,accel);
        accelerate(v,desired,accel,surface,airMode);
        CHECK(scene::length(v-Vec3{m.m_vecVelocity[0],m.m_vecVelocity[1],m.m_vecVelocity[2]})<.001f);
        const auto normal=scene::normalize(Vec3{std::cos(angle),std::sin(angle),.7f});
        source_sdk_oracle::Vector n{{normal.x,normal.y,normal.z}},out;
        sdk.ClipVelocity(m.m_vecVelocity,n,out,1,0);
        CHECK(scene::length(clip(v,normal)-Vec3{out[0],out[1],out[2]})<.002f);
    }
    Settings cfg;Command cmd;cmd.forward=1;
    State run;tick(run,cmd,cfg,floorTrace);CHECK(near(run.velocity.x,18.75f));CHECK(near(run.position.x,.28125f));
    for(int i=0;i<100;++i)tick(run,cmd,cfg,floorTrace);CHECK(near(run.velocity.x,250));
    State slow;slow.velocity={50,0,0};friction(slow);CHECK(near(slow.velocity.x,45.5f));
    Vec3 air{300,0,0};accelerate(air,{0,250,0},10,1,true);CHECK(near(air.x,300));CHECK(near(air.y,30));
    // Actual CSS target jump impulse and gravity ordering, not the old HL2MP
    // 268-unit impulse or GoldSrc 10 ms command schedule.
    State jump;Command j;j.jump=true;tick(jump,j,cfg,floorTrace);
    CHECK(near(jump.position.z,(301.993377f-12)*.015f));CHECK(near(jump.velocity.z,301.993377f-18));
    for(bool unlimited:{false,true})for(float sign:{-1.f,1.f}){
        State s;s.velocity={600*sign,0,0};Settings c=cfg;c.unlimitedBunnyhop=unlimited;
        tick(s,j,c,floorTrace);CHECK(near(s.velocity.x,sign*(unlimited?600.f:300.f*600/std::sqrt(360036.f))));
    }
    for(float z:{100.f,150.f,-100.f}){
        State s;s.position={0,0,1000};s.velocity.z=z;tick(s,cmd,cfg,empty);
        CHECK(near(s.velocity.x,z==100?9.375f:30.f));CHECK(near(s.velocity.z,z-12));
    }
    // Camera-facing reversal and strafing-key reversal yield identical world
    // commands. No special backward branch is part of the controller.
    for(bool unlimited:{false,true})for(float angle:{0.f,.37f,2.1f}){
        State a,b;a.velocity=b.velocity={250*std::cos(angle),250*std::sin(angle),0};Settings c=cfg;c.unlimitedBunnyhop=unlimited;
        int hops=0;float previousZ=0;
        for(int i=0;i<500;++i){
            Command f;f.jump=true;f.side=(i/25)%2?1.f:-1.f;f.yaw=std::atan2(a.velocity.y,a.velocity.x)+f.side*.025f;
            Command back=f;back.yaw+=scene::kPi;back.side=-back.side;
            tick(a,f,c,floorTrace);tick(b,back,c,floorTrace);
            CHECK(scene::length(a.velocity-b.velocity)<.02f);CHECK(scene::length(a.position-b.position)<.1f);
            if(previousZ<=0&&b.velocity.z>0)++hops;previousZ=b.velocity.z;
        }
        CHECK(hops>=9);CHECK(std::hypot(b.velocity.x,b.velocity.y)>250);
    }
    // Geometric wall/step/ceiling/surf checks through the production hull sweep.
    for(bool step:{false,true}){
        scene::glb::Map map;quad(map,{-5000,-5000,0},{5000,-5000,0},{5000,5000,0},{-5000,5000,0});
        const float height=step?12.f:100.f;
        quad(map,{100,-1000,0},{100,-1000,height},{100,1000,height},{100,1000,0});
        quad(map,{100,-1000,height},{1000,-1000,height},{1000,1000,height},{100,1000,height});map.buildCollisionIndex();
        auto trace=[&](Vec3 a,Vec3 b,float r=16,Vec3 offset=Vec3{}){auto t=gameplay::movement::sweep(map,a+offset,b+offset,r,72);t.end-=offset;return t;};
        State s;s.position.z=.01f;for(int i=0;i<80;++i)tick(s,cmd,cfg,trace);
        if(step){CHECK(s.position.x>150);CHECK(near(s.position.z,12,.05f));}else CHECK(s.position.x<=84.01f);
    }
    {
        scene::glb::Map map;quad(map,{-1000,-1000,0},{1000,-1000,0},{1000,1000,0},{-1000,1000,0});
        quad(map,{-1000,-1000,85},{-1000,1000,85},{1000,1000,85},{1000,-1000,85});
        auto trace=[&](Vec3 a,Vec3 b,float r=16,Vec3 offset=Vec3{}){auto t=gameplay::movement::sweep(map,a+offset,b+offset,r,72);t.end-=offset;return t;};
        State s;for(int i=0;i<60;++i){tick(s,j,cfg,trace);CHECK(s.position.z<=13.01f);}
    }
    for(float sign:{-1.f,1.f}){
        scene::glb::Map map;quad(map,{-5000,-5000,-10000},{5000,-5000,10000},{5000,5000,10000},{-5000,5000,-10000});
        auto trace=[&](Vec3 a,Vec3 b,float r=16,Vec3 offset=Vec3{}){auto t=gameplay::movement::sweep(map,a+offset,b+offset,r,72);t.end-=offset;return t;};
        State s;s.position={0,0,40};s.velocity={0,250*sign,0};Command neutral;
        for(int i=0;i<80;++i){tick(s,neutral,cfg,trace);CHECK(!s.grounded);}
        CHECK(s.velocity.x<-100);CHECK(near(s.velocity.y,250*sign));CHECK(s.position.z<0);
    }
    std::cout<<"PASS CSS scalar, jump, stock/uncapped backhop, deadstrafe, step, wall, ceiling and emergent surf checks\n";
}
