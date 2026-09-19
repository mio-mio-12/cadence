#include "gameplay/BotSpVariety.h"

#include <iostream>
#include <vector>

int main(){
    using namespace gameplay::bot;
    int failures=0;
    const auto check=[&](bool value,const char* message){if(!value){++failures;std::cerr<<message<<'\n';}};
    for(std::uint64_t i=0;i<1000;++i){
        check(!spOpportunityChance(0,7,i),"zero probability emitted a reaction");
        check(spOpportunityChance(1,7,i),"one probability skipped a reaction");
        check(spOpportunityChance(0.4f,7,i)==spOpportunityChance(0.4f,7,i),"deterministic chance changed");
    }
    const auto sequence=[](double dt,int frames){SpVarietyClock clock;std::vector<bool> result;for(int i=0;i<frames;++i)if(clock.advance(dt,0.5))result.push_back(spOpportunityChance(0.4f,7,clock.opportunitySequence));return result;};
    const auto normal=sequence(1.0/60,3600);
    check(normal.size()==120,"periodic clock emitted wrong count");
    check(normal==sequence(1.0/240,14400),"render frequency changed opportunity sequence");
    check(normal==sequence(0.1/60,36000),"0.1x changed equal-game-time opportunity sequence");
    check(normal==sequence(0.01/60,360000),"0.01x changed equal-game-time opportunity sequence");
    SpVarietyClock clock;
    check(!clock.advance(0,1)&&clock.opportunitySequence==0,"paused clock emitted opportunity");
    check(!clock.advance(-1,1)&&!clock.advance(1,0),"invalid clock input accepted");
    check(clock.advance(50.2,1)&&clock.opportunitySequence==1,"hitch did not emit exactly one opportunity");
    check(!clock.advance(0.1,1),"hitch backlog replayed an opportunity");
    SpMovingReactionContext context;context.gameDelta=0.01f;context.moving=true;
    SpMovingReactionChances chances{1,1,1};
    const auto pick=[&](){return chooseSpMovingReaction(context,chances,3,5);};
    check(pick()==SpMovingReaction::Slip,"safe quiet moving rifle did not select slip");
    context.targetVisible=true;check(pick()==SpMovingReaction::None,"slip occurred in visible combat");
    context.actualNearbyShotAge=0.1f;check(pick()==SpMovingReaction::NearMiss,"actual nearby shot did not select reaction");
    context.actualDamageAge=0.1f;check(pick()==SpMovingReaction::MovingPain,"actual damage did not take priority");
    chances.movingPain=0;check(pick()==SpMovingReaction::None,"rejected pain incorrectly fell through to near miss");chances.movingPain=1;
    for(bool* guard:{&context.mantling,&context.reloading,&context.firing,&context.equipment,&context.melee,&context.scenarioActive}){*guard=true;check(pick()==SpMovingReaction::None,"active action safety guard ignored");*guard=false;}
    for(bool* required:{&context.alive,&context.grounded,&context.standing,&context.rifle,&context.moving}){*required=false;check(pick()==SpMovingReaction::None,"required movement/rig guard ignored");*required=true;}
    context.gameDelta=0;check(pick()==SpMovingReaction::None,"paused selector emitted reaction");context.gameDelta=0.01f;
    context.actualDamageAge=1;context.actualNearbyShotAge=1;check(pick()==SpMovingReaction::None,"stale event emitted combat reaction");
    context.actualDamageAge=-0.1f;context.actualNearbyShotAge=-0.1f;check(pick()==SpMovingReaction::None,"negative event age accepted");
    check(spLocomotionPlaybackRate(100,100)==1,"equal speed playback altered");
    check(spLocomotionPlaybackRate(1000,100)==1.45f,"playback maximum not clamped");
    check(spLocomotionPlaybackRate(1,100)==0.65f,"playback minimum not clamped");
    check(spLocomotionPlaybackRate(100,0)==1,"missing authored speed changed playback");
    if(!failures)std::cout<<"SP variety safety, deterministic opportunities, 0.01x cadence and playback bounds passed.\n";
    return failures?1:0;
}
