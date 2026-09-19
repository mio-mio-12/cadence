#include "app/CatalogRemap.h"
#include "app/BotDeathPolicy.h"
#include "app/BotSpContextLibrary.inl"
#include "gameplay/BotActionPlayback.h"
#include "gameplay/BotActor.h"
#include <iostream>
#include <set>
#include <cstdlib>

static void check(bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
static std::optional<std::size_t> originalContext(const cadence::sp::ContextLibrary& library,const cadence::sp::ContextRequest& r,unsigned actor,unsigned sequence){
    float best=std::numeric_limits<float>::infinity();std::vector<std::size_t> choices;
    for(const auto& c:library){const auto& d=c.descriptor;if(!cadence::sp::contextCompatible(d,r))continue;
        float score=0;
        if(d.kind==cadence::sp::ContextKind::TraversalMantle||d.kind==cadence::sp::ContextKind::Jump)score=std::abs(d.heightIw-r.traversalHeightIw);
        if(d.kind==cadence::sp::ContextKind::StandAim||d.kind==cadence::sp::ContextKind::StandFire)score=std::abs(d.pitchDegrees-r.aimPitchDegrees);
        if(score<best-.001f){best=score;choices.clear();}if(std::abs(score-best)<.001f)choices.push_back(c.animation);
    }
    if(choices.empty())return {};
    return choices[(static_cast<std::uint64_t>(actor)*7919+sequence)%choices.size()];
}
int main(){
    {
        cadence::BotDeathHistory history;cadence::BotDeathRequest request;request.spMode=true;request.mix=1;
        std::vector<cadence::BotDeathChoice> choices;
        for(unsigned i=0;i<8;++i){cadence::BotDeathChoice c;c.animation=i;c.key="death_"+std::to_string(i);choices.push_back(c);}
        history.beginBatch(42);std::set<std::size_t> selected;
        for(unsigned i=0;i<8;++i){request.actor=i;const auto clip=cadence::selectBotDeath(choices,history,request);check(clip&&selected.insert(*clip).second,"same-shot deaths repeated");}
        check(!cadence::selectBotDeath(choices,history,request),"exhausted batch repeated a death");
        history.beginBatch(43);check(cadence::selectBotDeath(choices,history,request).has_value(),"next shot retained exhausted batch");
        scene::CastScene library;
        for(const char* name:{"pb_death_explosion_front","pb_death_grenade_back","pb_death_blast","ai_death_flatonback","pb_death_normal"}){
            scene::Animation a;a.sourceName=name;a.action=scene::ActionRole::Death;a.domain=std::string(name).starts_with("ai_")?scene::AnimationDomain::Other:scene::AnimationDomain::PlayerBody;a.tracks.resize(1);library.animations.push_back(a);
        }
        const auto safe=cadence::prepareBotDeaths(library);check(safe.size()==2&&safe[0].sp,"gun death pool included explosions or lost contextual SP death");
    }
    gameplay::bot::Actor life;life.id=18;life.modelVariant=2;life.weaponSlot=1;life.viewWeaponAsset=42;life.worldWeaponAsset=43;life.team="seal";life.weaponClass=scene::WeaponClass::Sniper;
    for(int death=0;death<1000;++death){
        life.alive=false;life.health=0;life.mantling=true;life.mantleElapsed=.4f;life.mantleEnd={900,900,900};life.stance=scene::Stance::Prone;
        life.spMovingScenario=true;life.spScenarioWeight=1;life.equipmentPoseTime=2;life.actionBlendWeight=1;life.reloadTime=2;life.grounded=false;life.velocity={100,20,-300};
        gameplay::bot::respawnActor(life,{10,20,30});
        check(life.alive&&life.health==100&&life.grounded&&!life.mantling&&life.mantleElapsed==0,"respawn retained dead/mantle physics");
        check(!life.spMovingScenario&&life.spScenarioWeight==0&&life.equipmentPoseTime==0&&life.actionBlendWeight==0&&life.reloadTime==0,"respawn retained action overlays");
        check(life.stance==scene::Stance::Stand&&scene::length(life.velocity)==0&&life.position.z==30,"respawn retained stance or trajectory");
        check(life.id==18&&life.modelVariant==2&&life.weaponSlot==1&&life.viewWeaponAsset==42&&life.worldWeaponAsset==43&&life.team=="seal"&&life.ammo==5,"respawn changed identity/loadout");
    }
    assets::Catalog old,next;
    for(const char* path:{"C:/rips/bo2/hands.cast","C:/rips/bo2/an94.cast","C:/rips/bo2/fiveseven.cast","C:/rips/mw/body.cast"}){assets::Asset a;a.path=path;old.entries.push_back(a);}
    next=old;std::reverse(next.entries.begin(),next.entries.end());assets::Asset added;added.path="C:/rips/bo2_sp/body.cast";next.entries.insert(next.entries.begin(),added);
    cadence::CatalogRemap remap(old,next);
    for(std::size_t i=0;i<old.entries.size();++i)check(next.entries[remap(i)].path==old.entries[i].path,"catalog asset identity changed");
    check(remap(cadence::CatalogRemap::missing)==cadence::CatalogRemap::missing,"invalid selection became valid");
    check(remap.find("c:/RIPS/BO2/../bo2/an94.cast")==remap(1),"path normalization failed");
    next.entries.clear();cadence::CatalogRemap removed(old,next);check(removed(1)==cadence::CatalogRemap::missing,"removed weapon retained stale index");

    scene::CastScene scene;scene::Bone bone;bone.name="root";scene.skeleton.bones.push_back(bone);
    scene::Animation reload;reload.action=scene::ActionRole::Reload;reload.domain=scene::AnimationDomain::PlayerTorso;reload.looping=true;reload.durationFrames=30;reload.framerate=30;
    scene::Track track;track.boneIndex=0;track.property=scene::TrackProperty::TranslationX;track.mode=scene::TrackMode::Absolute;track.frames={0,30};track.scalarValues={0,10};reload.tracks.push_back(track);
    scene.animations.push_back(reload);
    gameplay::bot::configureBotOneShots(scene);
    check(!scene.animations[0].looping,"reload remained looping");
    for(int i=30;i<120;++i)check(scene.sampleLocalPose(0,float(i))[0].position.x==10,"short reload wrapped rather than holding final pose");
    using scene::ActionRole;
    check(gameplay::bot::restartBotAction(true,true,ActionRole::Reload,true,10,30),"new reload did not restart");
    check(!gameplay::bot::restartBotAction(true,true,ActionRole::Fire,true,10,30),"automatic shot restarted cycle opening");
    check(gameplay::bot::restartBotAction(true,true,ActionRole::Fire,true,30,30),"completed cycle did not restart on next shot");
    check(gameplay::bot::restartBotAction(true,false,ActionRole::Fire,true,10,30),"new burst did not restart");
    check(gameplay::bot::restartBotAction(true,true,ActionRole::Fire,false,10,30),"discrete shot failed to restart");

    std::vector<cadence::BotDeathChoice> deaths;
    for(int i=0;i<12;++i)deaths.push_back({std::size_t(i),"death"+std::to_string(i),scene::MotionRole::Unknown,scene::Stance::Stand,scene::Direction::Any,scene::WeaponClass::Any,i>=10,i==0});
    cadence::BotDeathRequest r;r.spMode=true;r.mix=1;r.stance=scene::Stance::Stand;r.direction=scene::Direction::Any;r.weapon=scene::WeaponClass::Any;r.motion=scene::MotionRole::Unknown;
    cadence::BotDeathHistory history;std::array<std::size_t,2> last{99,99};std::set<std::size_t> seen;int promoted=0;
    for(unsigned i=0;i<20000;++i){r.actor=i%5;const auto choice=cadence::selectBotDeath(deaths,history,r);check(choice.has_value(),"full random death pool failed");check(*choice!=last[0]&&*choice!=last[1],"death repeated one of last two kills");last={*choice,last[0]};seen.insert(*choice);promoted+=*choice==0;}
    check(seen.size()==12,"mix 1 did not reach every supported death");check(promoted>20000/12,"running-forward boost was ineffective");
    deaths.resize(2);history={};r.mix=0;
    check(cadence::selectBotDeath(deaths,history,r).has_value(),"small pool first choice failed");
    check(cadence::selectBotDeath(deaths,history,r).has_value(),"small pool second choice failed");
    check(!cadence::selectBotDeath(deaths,history,r),"exhausted pool violated hard repeat exclusion");

    cadence::sp::ContextLibrary library;
    for(int copy=0;copy<80;++copy)for(const auto& d:cadence::sp::kContextDescriptors)library.push_back({library.size(),d});
    for(unsigned i=0;i<1000;++i){cadence::sp::ContextRequest request;request.kind=static_cast<cadence::sp::ContextKind>(i%10);request.stance=i%2?cadence::sp::ContextStance::Stand:cadence::sp::ContextStance::Crouch;request.moving=(i/2)%2;request.firing=(i/4)%2;request.coverValidated=request.traversalValidated=request.temporaryWounded=request.justDied=true;request.alive=request.kind!=cadence::sp::ContextKind::Death;request.coverHeightIw=44;request.traversalHeightIw=40;request.aimPitchDegrees=float(int(i%181)-90)+.0003f;
        check(originalContext(library,request,i%7,i)==cadence::sp::selectContextClip(library,request,i%7,i),"allocation-free context selection changed result");}
    for(float probability:{0.f,.25f,.5f,1.f}){int accepted=0;for(unsigned i=0;i<20000;++i)accepted+=gameplay::bot::spOpportunityChance(probability,5,i,21);check(std::abs(accepted-20000*probability)<300,"literal probability distribution failed");}
    std::cout<<"Catalog identities, reload endpoint, firing cycles, 20000 nonrepeating deaths, context parity and chance distribution passed\n";
}
