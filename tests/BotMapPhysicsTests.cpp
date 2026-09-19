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
int main(){
    using namespace gameplay::bot;
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
