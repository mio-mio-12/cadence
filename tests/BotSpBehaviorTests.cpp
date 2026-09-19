#include "gameplay/BotActor.h"
#include <iostream>

int main(){
    using namespace gameplay::bot;
    using gameplay::iw::worldUnits;
    {
        BehaviorConfig c;c.spPlayerPolicy=true;c.allowEquipment=true;c.equipmentChance=1;c.spUseSceneVisibilityOnly=true;
        Actor a;a.id=4;a.grounded=true;a.equipmentOpportunityTime=0;
        updateBehavior(a,{1000,0,0},c,.016f,nullptr,false);
        if(!a.input.equipment||a.input.fire||a.input.sprint){std::cerr<<"SP equipment opportunity missing or fired during equipment\n";return 1;}
        a.equipmentOpportunityTime=0;c.allowEquipment=false;
        updateBehavior(a,{1000,0,0},c,.016f,nullptr,false);
        if(a.input.equipment){std::cerr<<"disabled SP equipment emitted\n";return 1;}
    }
    int failures=0;
    const auto check=[&](bool good,const char* message){if(!good){++failures;std::cerr<<message<<'\n';}};
    BehaviorConfig config;config.spPlayerPolicy=true;config.spUseSceneVisibilityOnly=true;config.aimErrorDegrees=0;config.magazineSize=6;config.reloadDuration=0.5f;config.preferredRange=worldUnits(250);
    Actor bot;bot.id=1;bot.ammo=6;
    const scene::Vec3 player{worldUnits(220),0,0};
    int shots=0,bursts=0,repositions=0,reloads=0,stopFrames=0,movingShots=0;
    auto old=bot.spPhase;
    for(int i=0;i<2400;++i){
        const int before=bot.ammo;
        updateBehavior(bot,player,config,1.0f/120,nullptr,true);
        if(bot.input.fire){++shots;check(bot.ammo==before-1,"fire did not consume exactly one round");check(bot.spPhase==SpPhase::Burst,"shot outside burst decision");if(bot.spWantsMove){++movingShots;check(bot.input.ads&&!bot.input.sprint,"moving fire lost ADS or fired while sprinting");}}
        else if(!bot.input.reload)check(bot.ammo==before,"ammo spent without fire output");
        if(bot.reloadStarted)++reloads;
        if(bot.spPhase!=old){if(bot.spPhase==SpPhase::Burst)++bursts;if(bot.spPhase==SpPhase::Reposition)++repositions;old=bot.spPhase;}
        if(!bot.spWantsMove)++stopFrames;
        check(std::abs(bot.input.right)<=1.001f,"combat strafe exceeded normalized movement throttle");
    }
    check(shots>12&&bursts>3&&repositions>1&&reloads>1,"combat did not cycle burst/reload/reposition");
    check(stopFrames<1200&&movingShots>6,"combat still spends too long stopped or cannot fire while moving");
    const auto remembered=bot.lastKnownPlayer;
    for(int i=0;i<200;++i){updateBehavior(bot,{worldUnits(2000),worldUnits(500),0},config,1.0f/120,nullptr,false);check(!bot.input.fire,"shot through occluder");}
    check(horizontalDistance(remembered,bot.lastKnownPlayer)<0.001f,"search followed hidden player's true location");
    check(bot.routeCount==0&&bot.navigationRoute.empty(),"SP path used legacy course route");
    Actor idle;idle.id=5;int windows=0;auto idleConfig=config;idleConfig.spIdlePauseChance=1;
    for(int i=0;i<4800;++i){updateBehavior(idle,{worldUnits(3000),0,0},idleConfig,1.0f/120,nullptr,false);if(idle.spIdleWindow){++windows;check(!idle.input.ads&&!idle.input.fire&&!idle.spWantsMove,"idle opportunity overridden by combat/movement");}}
    check(windows>200,"no naturally reachable SP idle opportunities");
    idle.health-=10;updateBehavior(idle,{worldUnits(3000),0,0},config,1.0f/120,nullptr,false);
    check(idle.spReactionWindow&&!idle.input.fire,"damage did not offer reaction opportunity");
    Actor reload;reload.ammo=0;updateBehavior(reload,player,config,0.01f,nullptr,true);
    check(reload.reloadStarted&&reload.input.reload&&!reload.input.fire,"empty magazine failed to begin reload");
    for(int i=0;i<70;++i)updateBehavior(reload,player,config,0.01f,nullptr,true);
    check(reload.ammo>0&&reload.reloadTime<=0,"reload timer failed to advance through SP state");
    const auto frame=reload.behaviorClock;const auto cooldown=reload.fireCooldown;
    updateBehavior(reload,player,config,0,nullptr,true);
    check(reload.behaviorClock==frame&&reload.fireCooldown==cooldown,"paused policy advances game time");
    Actor moving;moving.spPolicyInitialized=true;moving.spPhase=SpPhase::Burst;moving.spBurstShots=3;moving.spPhaseTime=1;
    moving.spMovingScenario=true;moving.spMovingPain=true;moving.spMotionForward=.4f;moving.spMotionYaw=.1f;
    moving.ammo=6;moving.targetVisible=true;moving.targetVisibleTime=10;
    updateBehavior(moving,player,config,.01f,nullptr,true);
    check(!moving.input.fire&&moving.ammo==6&&moving.input.forward==.4f,"moving reaction failed to own movement/fire");
    moving.spScenarioInterrupted=true;
    updateBehavior(moving,player,config,.01f,nullptr,true);
    check(!moving.input.fire&&moving.ammo==6&&moving.input.forward==0,"moving reaction blend-out fired or moved");
    Actor patrol;patrol.id=4;auto patrolConfig=config;patrolConfig.spPatrolWalkChance=1;patrolConfig.spIdlePauseChance=0;
    updateBehavior(patrol,{worldUnits(3000),0,0},patrolConfig,.01f,nullptr,false);
    check(patrol.spPatrolWalking&&patrol.spMoveThrottle>0&&patrol.spMoveThrottle<.1f,"patrol did not choose walking or started instantly at full speed");
    for(int i=0;i<4800;++i){updateBehavior(patrol,{worldUnits(3000),0,0},patrolConfig,1.f/120,nullptr,false);check(!patrol.spIdleWindow&&!patrol.input.sprint,"zero pause chance stopped patrol or walking sprinted");}
    check(patrol.spMoveThrottle<=.421f,"walk throttle exceeded walking speed");
    Actor sprint;sprint.spWantsMove=true;sprint.spSprintWanted=true;sprint.spMoveThrottle=1;sprint.spPhase=SpPhase::Search;sprint.spMoveGoal={worldUnits(500),0,0};
    steerSpMovement(sprint,{worldUnits(50),0,0},.01f);
    check(sprint.input.sprint,"short route waypoint suppressed long search sprint");
    Actor jumper;jumper.spWantsMove=true;jumper.input.forward=1;auto jumpConfig=config;jumpConfig.spJumpChance=1;
    check(!applySpTacticalJump(jumper,jumpConfig,.01f,true),"idle/patrol jumped without combat or traversal intent");
    jumper.targetVisible=jumper.spCombatMoving=true;jumper.velocity={200,0,0};
    check(!applySpTacticalJump(jumper,jumpConfig,.01f,false),"unapproved geometry caused jump");
    check(applySpTacticalJump(jumper,jumpConfig,.01f,true)&&jumper.input.jump,"approved moving geometry did not jump at 100 percent");
    jumper.input.jump=false;jumper.spJumpCooldown=0;
    check(applySpTacticalJump(jumper,jumpConfig,.01f,true)&&jumper.input.jump,"100 percent chance failed a newly validated grounded opportunity after recovery");
    applySpTacticalJump(jumper,jumpConfig,.01f,false);jumper.grounded=false;
    check(!applySpTacticalJump(jumper,jumpConfig,.01f,true),"airborne jump was retriggered");
    applySpTacticalJump(jumper,jumpConfig,.01f,false);jumper.grounded=true;jumpConfig.spJumpChance=0;
    check(!applySpTacticalJump(jumper,jumpConfig,.01f,true),"zero jump chance jumped");
    applySpTacticalJump(jumper,jumpConfig,.01f,false);jumpConfig.spJumpChance=1;
    check(!applySpTacticalJump(jumper,jumpConfig,0,true),"paused policy jumped");
    jumper.reloadTime=1;check(!applySpTacticalJump(jumper,jumpConfig,.01f,true),"reload ownership allowed tactical jump");
    if(!failures)std::cout<<"SP policy: "<<shots<<" shots ("<<movingShots<<" moving), "<<bursts<<" bursts, "<<repositions<<" repositions, "<<reloads<<" reloads; "<<stopFrames<<"/2400 stationary combat frames; "<<windows<<" forced-100%-chance idle-window samples. PASS\n";
    return failures?1:0;
}
