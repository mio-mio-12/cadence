#include "gameplay/BotActor.h"

#include <cmath>
#include <iostream>

int main(){
    int failures{};
    const auto expect=[&](bool value,const char* message){if(!value){std::cerr<<message<<'\n';++failures;}};
    gameplay::bot::Actor actor;actor.position={0,0,0};actor.input.forward=1;
    for(int i=0;i<125;++i)gameplay::bot::step(actor,1.0f/125.0f);
    expect(actor.position.x>gameplay::iw::worldUnits(100)&&actor.position.x<gameplay::iw::worldUnits(220),"bot did not use IW forward movement at CAST world scale");
    expect(std::abs(actor.position.y)<0.01f,"bot forward input drifted sideways");
    actor.input.forward=0;actor.input.jump=true;gameplay::bot::step(actor,1.0f/60.0f);
    expect(!actor.grounded&&actor.velocity.z>0,"bot jump did not leave ground");
    gameplay::bot::Actor thinker;gameplay::bot::BehaviorConfig config;config.detectionRange=gameplay::iw::worldUnits(500);config.preferredRange=gameplay::iw::worldUnits(100);config.aimResponse=20;
    for(int i=0;i<60;++i)gameplay::bot::updateBehavior(thinker,{gameplay::iw::worldUnits(120),0,0},config,1.0f/60.0f);
    expect(thinker.behavior==gameplay::bot::BehaviorState::Engage,"bot did not acquire player");
    expect(thinker.input.ads,"bot did not aim inside engagement range");
    expect(thinker.firePoseTime>0,"bot did not fire after aligning");
    gameplay::bot::Actor equipmentLocked;equipmentLocked.input.forward=1.0f;equipmentLocked.equipmentPoseTime=1.0f;gameplay::bot::BehaviorConfig equipmentConfig;equipmentConfig.detectionRange=0;equipmentConfig.allowEquipment=false;gameplay::bot::updateBehavior(equipmentLocked,{10000,0,0},equipmentConfig,1.0f/60.0f);expect(equipmentLocked.input.forward==0.0f&&!equipmentLocked.input.sprint,"equipment pose did not lock bot locomotion");
    gameplay::bot::updateBehavior(thinker,{gameplay::iw::worldUnits(300),0,0},config,1.0f/60.0f);
    expect(thinker.behavior==gameplay::bot::BehaviorState::Chase&&!thinker.targetVisible,"bot fired through a solid course obstacle");
    expect(thinker.routeCount>0,"bot did not build an obstacle-aware pursuit route");
    gameplay::bot::updateBehavior(thinker,{gameplay::iw::worldUnits(1200),0,0},config,1.0f/60.0f);
    expect(thinker.behavior==gameplay::bot::BehaviorState::Chase,"bot discarded its last-known-player search immediately");for(int i=0;i<360;++i)gameplay::bot::updateBehavior(thinker,{gameplay::iw::worldUnits(1200),0,0},config,1.0f/60.0f);expect(thinker.behavior==gameplay::bot::BehaviorState::Patrol,"bot did not forget the player and return to navigation");
    gameplay::bot::NavigationGraph navigation;navigation.nodes={{{0,0,0},1,1,gameplay::iw::worldUnits(48),false,false},{{gameplay::iw::worldUnits(200),0,0},3,1,gameplay::iw::worldUnits(48),true,false}};navigation.links.push_back({0,1,1,true});navigation.blocks.push_back({{gameplay::iw::worldUnits(400),-20,-20},{gameplay::iw::worldUnits(500),20,200}});expect(gameplay::bot::nextNavigationNode(navigation,0,1)==1,"weighted priority navigation link was not selected");expect(gameplay::bot::navigationBlocked(navigation,{gameplay::iw::worldUnits(450),0,0}),"blocked navigation volume was ignored");gameplay::bot::Actor guided;guided.navigation=&navigation;gameplay::bot::BehaviorConfig guidedConfig;guidedConfig.detectionRange=gameplay::iw::worldUnits(10);gameplay::bot::updateBehavior(guided,{gameplay::iw::worldUnits(1000),0,0},guidedConfig,1.0f/60.0f);expect(guided.waypoint==1,"authored navigation graph did not advance patrol through its link");expect(guided.movementIntentTime>0.0f,"navigation movement intent was not retained for locomotion presentation");
    gameplay::bot::NavigationGraph routed;routed.nodes.resize(4);routed.nodes[0].position={0,0,0};routed.nodes[1].position={100,0,0};routed.nodes[2].position={200,0,0};routed.nodes[3].position={100,100,0};routed.links={{0,1,1,true},{1,2,1,true},{0,3,10,true},{3,2,10,true}};gameplay::bot::Actor routeActor;gameplay::bot::buildNavigationRoute(routeActor,routed,0,2);expect(routeActor.navigationRoute.size()==3&&routeActor.navigationRoute[1]==1,"authored A-star route did not prefer the stable low-cost path");
    gameplay::bot::NavigationGraph attracted;attracted.nodes.resize(3);attracted.nodes[0].position={0,0,0};attracted.nodes[1].position={100,0,0};attracted.nodes[2].position={-100,0,0};attracted.links={{0,1,1,true},{0,2,1,true}};expect(gameplay::bot::nextNavigationNodeAttracted(attracted,0,1,99,{1000,0,0},1.0f)==1,"player encounter pull did not bias a patrol branch without awareness");
    gameplay::bot::NavigationGraph deadEnd;deadEnd.nodes.resize(4);deadEnd.nodes[0].position={0,0,0};deadEnd.nodes[1].position={100,0,0};deadEnd.nodes[2].position={0,100,0};deadEnd.nodes[3].position={0,200,0};deadEnd.nodes[1].priority=true;deadEnd.links={{0,1,1,true},{0,2,1,true},{2,3,1,true}};expect(gameplay::bot::nextNavigationNode(deadEnd,0,7)==2,"patrol chose a high-weight dead end despite a through route");expect(gameplay::bot::nextNavigationNodeAttracted(deadEnd,0,7,99,{1000,0,0},1.0f)==2,"player pull overrode dead-end rejection");
    expect(gameplay::iw::kStandingActorHeight*gameplay::iw::kUnitsToMeters>1.82f,"metric actor height conversion changed");
    {
        gameplay::bot::Actor wallActor;
        wallActor.position = {scene::course::scaled(240.0f) - scene::course::kPlayerRadius, 0, 0};
        wallActor.yaw = 0.0f;
        wallActor.input.forward = 1.0f;
        for(int i=0; i<80; ++i) gameplay::bot::step(wallActor, 1.0f / 125.0f);
        expect(wallActor.routeDiversionTime > 0.0f, "bot did not abandon blocked route after running into wall");
        expect(wallActor.blockedHeadingMemory > 0.0f, "bot did not remember blocked heading to prevent snapping back");
        expect(std::abs(gameplay::bot::wrapAngle(wallActor.diversionYaw - 0.0f)) > 1.0f, "bot diversion did not steer away from wall");
        expect(std::abs(gameplay::bot::wrapAngle(wallActor.yaw - 0.0f)) < 0.1f, "bot snap-rotated instantly instead of turning naturally");
    }
    {
        // Test interval XY stuck tracker: horizontal progress blocked even if Z varies
        gameplay::bot::Actor stuckActor;
        stuckActor.position = {100, 100, 0};
        stuckActor.yaw = 0.0f;
        stuckActor.input.forward = 1.0f;
        stuckActor.grounded = true;
        // Run for 0.6 seconds (stuck interval is 0.5s) holding actor in place horizontally
        for(int i=0; i<75; ++i) {
            gameplay::bot::step(stuckActor, 1.0f / 125.0f, 1.0f, false);
            stuckActor.position.x = 100.0f;
            stuckActor.position.y = 100.0f;
            stuckActor.position.z += 1.0f; // Z moves (e.g. lift/slope), but XY is zero
        }
        expect(stuckActor.routeDiversionTime > 0.0f, "interval XY stuck tracker did not detect lack of horizontal progress");
        expect(stuckActor.blockedHeadingMemory > 0.0f, "interval XY stuck tracker did not set blocked heading memory");
    }
    {
        // Test playerAttraction chase pull
        gameplay::bot::Actor hunter;
        hunter.position = {0, 0, 0};
        gameplay::bot::BehaviorConfig hunterConfig;
        hunterConfig.playerAttraction = 1.0f;
        hunterConfig.detectionRange = gameplay::iw::worldUnits(10); // Far beyond line of sight
        gameplay::bot::updateBehavior(hunter, {gameplay::iw::worldUnits(500), 0, 0}, hunterConfig, 1.0f / 60.0f);
        expect(hunter.behavior == gameplay::bot::BehaviorState::Chase, "playerAttraction=1.0 did not promote bot to Chase state");
        expect(hunter.awareness >= 0.8f, "playerAttraction=1.0 did not raise awareness for pursuit");
    }
    if(!failures)std::cout<<"All bot actor tests passed.\n";
    return failures?1:0;
}
