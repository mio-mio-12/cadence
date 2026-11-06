#include "gameplay/BotMapPhysics.h"
#include "gameplay/BotPresentation.h"
#include <iostream>
#include <cstdlib>
static void check(bool v){if(!v)std::abort();}
struct Floor {
    float floor{};bool exists{true};bool wall{};
    scene::Vec3 constrainMove(scene::Vec3,scene::Vec3 p,float,float,float,scene::Vec3* n)const{if(wall&&p.x>30){p.x=30;*n={-1,0,0};}return p;}
    float navigationGroundHeight(float,float,float,float fallback,float)const{return exists?floor:fallback;}
};
static void quad(scene::glb::Map& map,scene::Vec3 a,scene::Vec3 b,scene::Vec3 c,scene::Vec3 d){
    for(auto points:{std::array{a,b,c},std::array{a,c,d}}){scene::glb::CollisionTriangle t;t.a=points[0];t.b=points[1];t.c=points[2];t.normal=scene::normalize(scene::cross(t.b-t.a,t.c-t.a));t.minimum=t.maximum=t.a;
        for(auto p:points){t.minimum={std::min(t.minimum.x,p.x),std::min(t.minimum.y,p.y),std::min(t.minimum.z,p.z)};t.maximum={std::max(t.maximum.x,p.x),std::max(t.maximum.y,p.y),std::max(t.maximum.z,p.z)};}
        t.walkable=t.normal.z>=.7f;t.blocking=!t.walkable;map.collision.push_back(t);
    }
}
static void actualMapFootprintChecks(){
    using namespace gameplay::bot;
    for(bool offCenter:{false,true}){
        scene::glb::Map map;quad(map,{-500,-500,0},{500,-500,0},{500,500,0},{-500,500,0});
        const float y0=offCenter?25.f:-100.f,y1=offCenter?35.f:100.f,x1=offCenter?110.f:300.f;
        quad(map,{100,y0,10},{x1,y0,10},{x1,y1,10},{100,y1,10});
        quad(map,{100,y0,0},{100,y0,10},{100,y1,10},{100,y1,0});map.buildCollisionIndex();
        Actor actor;actor.collisionHalfExtents=offCenter?scene::Vec2{12,43}:scene::Vec2{43,12};actor.standingCollisionHeight=100;
        actor.grounded=true;actor.spMovingScenario=true;actor.input.forward=95.f/gameplay::iw::kRunSpeed;
        for(int i=0;i<125;++i){stepOnMap(actor,.008f,1,map);check(actor.position.z>=-.01f);}
        check(actor.position.x>94&&actor.position.z>9.9f&&actor.grounded);
        // A descending body cannot snap early onto the same off-center top.
        actor.position={95,0,50};actor.velocity={0,0,-1};actor.grounded=false;actor.input={};actor.spMovingScenario=false;
        stepOnMap(actor,.008f,1,map);check(actor.position.z>49&&!actor.grounded);
        for(int i=0;i<150;++i)stepOnMap(actor,.008f,1,map);
        check(actor.grounded&&std::abs(actor.position.z-10)<.1f);
        actor.position={95,0,50};actor.velocity={};actor.grounded=false;
        for(int i=0;i<150;++i)stepCorpseOnMap(actor,.008f,map);
        check(actor.grounded&&std::abs(actor.position.z-10)<.1f);
    }
    scene::glb::Map ceiling;quad(ceiling,{-200,-200,0},{200,-200,0},{200,200,0},{-200,200,0});
    quad(ceiling,{-200,-200,110},{200,-200,110},{200,200,110},{-200,200,110});ceiling.buildCollisionIndex();
    Actor jumper;jumper.collisionHalfExtents={12,43};jumper.standingCollisionHeight=100;jumper.grounded=true;jumper.input.jump=true;
    bool hitCeiling=false;for(int i=0;i<100;++i){stepOnMap(jumper,.008f,1,ceiling);check(jumper.position.z<=10.1f);hitCeiling|=jumper.position.z>9&&jumper.velocity.z==0;}
    check(hitCeiling&&jumper.grounded&&std::abs(jumper.position.z)<.1f);
}
int main(){
    using namespace gameplay::bot;
    actualMapFootprintChecks();
    struct ExtentSpy:Floor {
        mutable scene::Vec2 used{};mutable float usedHeight{};
        using Floor::constrainMove;
        scene::Vec3 constrainMove(scene::Vec3,scene::Vec3 p,scene::Vec2 extents,float height,float,scene::Vec3*)const{used=extents;usedHeight=height;return p;}
    } extentSpy;
    Actor rectangular;rectangular.collisionHalfExtents={12,43};rectangular.standingCollisionHeight=210;rectangular.position.z=50;rectangular.grounded=false;
    stepOnMap(rectangular,.008f,1,extentSpy);check(extentSpy.used.x==12&&extentSpy.used.y==43&&extentSpy.usedHeight==210);
    extentSpy.used={};stepCorpseOnMap(rectangular,.008f,extentSpy);check(extentSpy.used.x==12&&extentSpy.used.y==43);
    respawnActor(rectangular,{});check(rectangular.collisionHalfExtents.x==12&&rectangular.collisionHalfExtents.y==43&&rectangular.standingCollisionHeight==210);
    struct HeightSpy:Floor {
        mutable float usedHeight{};
        scene::Vec3 constrainMove(scene::Vec3,scene::Vec3 p,float,float height,float,scene::Vec3*)const{usedHeight=height;return p;}
    } spy;
    for(float standing:{100.f,260.f})for(auto stance:{scene::Stance::Stand,scene::Stance::Crouch,scene::Stance::Prone}){
        Actor sized;sized.standingCollisionHeight=standing;sized.stance=stance;sized.position.z=50;sized.grounded=false;
        stepOnMap(sized,.008f,1,spy);check(std::abs(spy.usedHeight-collisionHeight(sized))<.001f);
        stepCorpseOnMap(sized,.008f,spy);check(std::abs(spy.usedHeight-collisionHeight(sized))<.001f);
        respawnActor(sized,{});check(sized.standingCollisionHeight==standing);
    }
    struct ExcessStep:Floor {scene::Vec3 constrainMove(scene::Vec3,scene::Vec3 p,float,float,float,scene::Vec3*)const{p.z=100;return p;}} excess;
    Actor limited;limited.grounded=true;limited.input.forward=1;
    for(int i=0;i<125;++i){stepOnMap(limited,.008f,1,excess);check(limited.position.z==0&&limited.grounded);}
    struct Edge:Floor {float navigationGroundHeight(float x,float,float,float fallback,float)const{return x<30?0:fallback;}} edge;
    Actor edgeBot;edgeBot.grounded=true;edgeBot.input.forward=1;
    for(int i=0;i<250;++i)stepOnMap(edgeBot,.008f,1,edge);
    check(edgeBot.grounded&&edgeBot.position.x<30&&edgeBot.position.z==0);
    edgeBot.input.jump=true;stepOnMap(edgeBot,.008f,1,edge);check(!edgeBot.grounded&&edgeBot.velocity.z>0);
    Actor b;b.position={0,0,50};b.grounded=false;Floor map;
    struct OverlaidFloor:Floor {float navigationGroundHeight(float,float,float reference,float fallback,float)const{const float cap=reference+45;return cap>=402.389f?402.389f:cap>=400.007f?400.007f:fallback;}} overlay;
    Actor falling;falling.position={0,0,401.82f};falling.velocity.z=-48.768f;falling.grounded=false;
    for(int i=0;i<100;++i){stepOnMap(falling,.008f,1,overlay);check(falling.position.z>=400.f);}
    check(falling.grounded);
    Actor corpse;corpse.position={0,0,401.82f};corpse.velocity.z=-48.768f;corpse.grounded=false;
    for(int i=0;i<100;++i){stepCorpseOnMap(corpse,.016f,overlay);check(corpse.position.z<=401.82f&&corpse.position.z>=400.f);}
    check(corpse.grounded);
    corpse={};corpse.position.z=-900;corpse.grounded=false;Floor absent;absent.exists=false;
    stepCorpseOnMap(corpse,.016f,absent);check(corpse.position.z< -900&&!corpse.grounded);
    stepOnMap(b,.008f,1,map);check(b.position.z>49&& !b.grounded); // No 56cm early ground snap.
    for(int i=0;i<200;++i)stepOnMap(b,.008f,1,map);check(b.grounded&&std::abs(b.position.z)<.001f);
    b.position.z=50;b.grounded=true;map.exists=false;stepOnMap(b,.008f,1,map);check(!b.grounded&&b.position.z<50);
    map.exists=true;map.wall=true;b={};b.grounded=true;b.input.forward=1;
    for(int i=0;i<100;++i){stepOnMap(b,1.f/60,1,map);check(b.position.x<=30.001f&&b.previousPhysicsPosition.x<=30.001f);}
    check(std::abs(b.velocity.x)<.001f);
    const auto before=b.position;stepOnMap(b,0,1,map);check(scene::length(before-b.position)<.001f);
    b={};b.grounded=true;b.input.jump=true;map.wall=false;stepOnMap(b,.008f,1,map);check(!b.grounded&&b.velocity.z>0);
    auto displayed=presentationPosition(b);float biggest=0;
    b.input.jump=false;
    for(int i=0;i<160;++i){stepOnMap(b,1.f/60,1,map);const auto now=presentationPosition(b);biggest=std::max(biggest,scene::length(now-displayed));displayed=now;}
    check(b.grounded&&biggest<15); // No early-landing 50–60cm discontinuity.
    b={};b.grounded=true;b.input.forward=1;displayed=b.position;
    for(int i=0;i<2000;++i){stepOnMap(b,.01f/120,1,map);const auto now=presentationPosition(b);check(scene::length(now-displayed)<1);displayed=now;}
    check(b.position.x>1);
    // Moving SP root travel must not be erased by the minimum walking stop
    // friction. The animation owns horizontal velocity, never position.
    for(float direction:{1.f,-1.f}){
        b={};b.grounded=true;b.spMovingScenario=true;
        const float desired=direction*25.f;
        b.input.forward=desired/(gameplay::iw::kRunSpeed*(direction<0?gameplay::iw::kBackSpeedScale:1.f));
        for(int i=0;i<125;++i){stepOnMap(b,.008f,1,map);check(std::abs(b.velocity.x-desired)<.001f);}
        check(std::abs(b.position.x-desired)<.01f);
    }
    b={};b.grounded=true;b.spMovingScenario=true;b.input.forward=25.f/gameplay::iw::kRunSpeed;
    map.wall=true;
    for(int i=0;i<250;++i){stepOnMap(b,.008f,1,map);check(b.position.x<=30.001f&&b.previousPhysicsPosition.x<=30.001f);}
    check(std::abs(b.position.x-30.f)<.001f&&std::abs(b.velocity.x)<.001f);
    map.wall=false;b={};b.grounded=true;b.spMovingScenario=true;b.input.forward=25.f/gameplay::iw::kRunSpeed;
    stepOnMap(b,.008f,1,map);check(std::abs(b.velocity.x-25.f)<.001f);
    b.spScenarioInterrupted=true;stepOnMap(b,.008f,1,map);
    check(b.velocity.x<24.f); // Interrupted fade returns ownership to ordinary movement.
    b.input.forward=0;for(int i=0;i<25;++i)stepOnMap(b,.008f,1,map);
    check(std::abs(b.velocity.x)<.001f);
    std::cout<<"SP low-speed/reverse root travel, collision, and interruption ownership passed\n";
    std::cout<<"fixed map history, no early landing, absent floor, contact velocity, pause/jump passed\n";
}
