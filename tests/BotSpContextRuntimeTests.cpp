#include "gameplay/BotActor.h"
#include "gameplay/SprintMotion.h"
#include "gameplay/BotMapRouting.h"
#include "app/BotSpContextLibrary.inl"
#include <iostream>

// Run the actual application context helpers with a deterministic local cover
// fixture. Rendering/real imported-map verification is a separate diagnostic.
struct ContextMap {
    struct RaycastHit {scene::Vec3 position{},normal{};float distance{};};
    mutable unsigned surfaceQueries{};bool thinFace{};
    std::optional<RaycastHit> raycastSurface(scene::Vec3 origin,scene::Vec3 direction,float)const{++surfaceQueries;if(thinFace)return RaycastHit{origin+direction*70.14f,{-1,0,0},70.14f};return {};}
    bool covered{true},shotBlocked{},nearbyOnly{},capsuleBlocked{};float height{120};mutable float lastShotHeight{};mutable unsigned groundQueries{};
    float navigationGroundHeight(float x,float y,float ceiling,float,float)const{++groundQueries;if(thinFace)return std::abs(x-72.14f)<.1f?height:0.f;return covered&&ceiling>100&&(!nearbyOnly||std::hypot(x-65.f,y)>90.f)?height:0.f;}
    bool navigationSegmentClear(scene::Vec3,scene::Vec3,float,float,float)const{return !capsuleBlocked;}
    scene::Vec3 constrainMove(scene::Vec3,scene::Vec3 desired,float,float,float)const{return capsuleBlocked?desired+scene::Vec3{50,0,0}:desired;}
    bool lineOfSight(scene::Vec3 from,scene::Vec3 to)const{
        if(thinFace)return from.z>height||to.x<70.14f;
        if(scene::length(to-from)>100.f){lastShotHeight=from.z;if(shotBlocked)return false;}
        return !covered||from.z>height;
    }
};
struct AppState {
    struct SpContextState {int varietyGrant{-1};unsigned contextCursor{},actionsStarted{},actionsCompleted{};float throwSettle{};gameplay::bot::SpVarietyClock clock;float wounded{},cover{},cooldown{},frame{},weight{},throwTime{},lastHealth{100},coverHeightIw{},coverSeek{},coverCheck{},coverStall{},coverBestDistance{};scene::Vec3 coverGoal{};std::size_t overlay{static_cast<std::size_t>(-1)},throwClip{static_cast<std::size_t>(-1)};unsigned sequence{},woundedSequence{};std::size_t coverClip{static_cast<std::size_t>(-1)};float coverElapsed{};
        std::size_t previousOverlay{static_cast<std::size_t>(-1)};float previousFrame{},transitionElapsed{.16f};
    };
    int activeBotSystemMode{1};
    std::size_t botRouteWorkTurn{};
    std::optional<scene::CastScene> botActorScene{scene::CastScene{}};
    cadence::sp::ContextLibrary botSpContextClips;
    std::vector<float> botClipAuthoredSpeeds;
    std::vector<gameplay::bot::Actor> bots{1};
    std::vector<SpContextState> botSpContextStates;
    gameplay::bot::NavigationGraph botNavigation;
    std::vector<gameplay::bot::BotMapRoute> botSpRoutes;
    std::optional<ContextMap> loadedMap;
    scene::Vec3 actorPosition{600,0,0};float actorViewHeight{150};
    float botSpWoundedChance{},botSpGrenadeChance{},botSpCoverChance{};
};
gameplay::bot::NavigationGraph& activeBotNavigation(AppState& app){return app.botNavigation;}
#include "app/BotSpContextRuntime.inl"

int main(){
    using namespace cadence::sp;using namespace gameplay::bot;
    int failures=0;const auto check=[&](bool pass,const char* msg){if(!pass){++failures;std::cerr<<msg<<'\n';}};
    AppState app;
    for(const auto& descriptor:kContextDescriptors){scene::Animation clip;clip.durationFrames=90;clip.framerate=30;clip.looping=descriptor.looping;app.botSpContextClips.push_back({app.botActorScene->animations.size(),descriptor});app.botActorScene->animations.push_back(clip);app.botClipAuthoredSpeeds.push_back(303.f);}
    auto& bot=app.bots[0];bot.id=1;bot.ammo=6;bot.weaponClass=scene::WeaponClass::Rifle;
    app.botSpGrenadeChance=1;spContextState(app,bot).clock.elapsed=20;
    app.botRouteWorkTurn=1;
    check(!prepareSpContext(app,bot,.01f,false)&&spContextState(app,bot).throwTime==0,"context opportunity ignored staggered work turn");
    check(spContextState(app,bot).clock.elapsed==1,"context opportunity did not remain banked");
    app.botRouteWorkTurn=0;
    bot.firePoseTime=5.f;
    check(prepareSpContext(app,bot,.01f,false),"100-percent quiet throw failed to own input");
    check(bot.firePoseTime==0,"new throw retained stale fire pose over its authored animation");
    check(!bot.input.fire&&!bot.input.equipment&&bot.ammo==6,"cosmetic throw generated shot/grenade gameplay event");
    for(int i=0;i<80;++i){check(prepareSpContext(app,bot,.01f,false),"throw ownership ended early");updateSpContextPose(app,bot,.01f);check(bot.ammo==6&&!bot.input.fire&&!bot.input.equipment,"throw consumed ammo or fired");}
    auto& throwState=spContextState(app,bot);const float heldFrame=throwState.frame,heldWeight=throwState.weight;
    updateSpContextPose(app,bot,0);check(throwState.frame==heldFrame&&throwState.weight==heldWeight,"delta-zero forced pose was changed");
    check(prepareSpContext(app,bot,.01f,true)&&throwState.throwTime>0,"visible target cancelled committed cosmetic throw");
    app.botSpGrenadeChance=0;app.botSpWoundedChance=1;throwState={};throwState.clock.elapsed=20;bot.velocity={150,0,0};bot.input.forward=1;
    check(prepareSpContext(app,bot,.01f,false)&&throwState.wounded>0&&bot.health==100,"ambient wounded acting did not reserve movement or changed health");
    const auto wounded=spContextBaseAnimation(app,bot);check(wounded.has_value(),"temporary wounded moving base not chosen");
    ++throwState.woundedSequence;const auto nextWounded=spContextBaseAnimation(app,bot);
    check(nextWounded&&nextWounded!=wounded,"successive wounded episodes stayed locked to one clip");
    check(spContextBaseAnimation(app,bot)==nextWounded,"wounded clip changed within the same episode");
    --throwState.woundedSequence;
    bot.input.forward=1;bot.input.right=.1f;bot.input.sprint=bot.sprintIntent=true;const auto velocityBefore=bot.velocity;
    limitSpWoundedMovement(app,bot);
    check(std::abs(std::hypot(bot.input.forward,bot.input.right)*gameplay::iw::kRunSpeed-303.f)<.01f&&std::abs(bot.input.right/bot.input.forward-.1f)<.001f,"wounded movement did not cap authored speed while preserving direction");
    check(!bot.input.sprint&&!bot.sprintIntent&&scene::length(bot.velocity-velocityBefore)<.001f,"wounded movement changed velocity directly or retained sprint");
    bot.input.forward=.3f;bot.input.right=0;limitSpWoundedMovement(app,bot);check(bot.input.forward==.3f,"wounded cap accelerated a slower patrol");
    if(wounded){app.botClipAuthoredSpeeds[*wounded]=371;bot.input.forward=1;limitSpWoundedMovement(app,bot);check(std::abs(bot.input.forward*gameplay::iw::kRunSpeed-371)<.01f,"wounded cap ignored the selected clip speed");}
    for(int guard=0;guard<5;++guard){
        bot.input.forward=1;bot.input.right=0;bot.input.sprint=true;
        bot.input.fire=guard==0;bot.reloadTime=guard==1?.5f:0;bot.spScenarioWeight=guard==2?1.f:0;bot.grounded=guard!=3;app.activeBotSystemMode=guard==4?0:1;
        limitSpWoundedMovement(app,bot);check(bot.input.forward==1&&bot.input.sprint,"wounded cap changed firing/reload/scenario/airborne/Classic movement");
    }
    bot.input.fire=false;bot.reloadTime=0;bot.spScenarioWeight=0;bot.grounded=true;app.activeBotSystemMode=1;
    app.botSpWoundedChance=0;for(int i=0;i<600;++i)prepareSpContext(app,bot,.01f,false);
    check(throwState.wounded==0&&!spContextBaseAnimation(app,bot),"wounded acting did not expire");
    app.loadedMap=ContextMap{};app.botSpCoverChance=1;throwState={};throwState.clock.elapsed=20;bot.velocity={};bot.input={};bot.targetVisible=true;
    app.loadedMap->thinFace=true;
    const auto thinHeight=spCoverHeightAt(app,bot.position);
    check(thinHeight&&std::abs(*thinHeight-120.f/gameplay::iw::kWorldUnitsPerIwUnit)<.01f,"thin face beyond old 70cm endpoint was rejected");
    app.loadedMap->thinFace=false;
    check(!prepareSpContext(app,bot,.01f,true)&&throwState.coverSeek>0,"validated nearby cover failed to activate seek");
    {BehaviorConfig config;config.spPlayerPolicy=true;config.allowEquipment=true;config.equipmentChance=1;
     bot.equipmentOpportunityTime=0;reserveSpContextPolicy(app,bot,config);
     updateBehavior(bot,app.actorPosition,config,.01f,nullptr,false);
     check(!config.allowEquipment&&!bot.input.equipment,"normal equipment policy stole newly acquired cover reservation");
     const float heldSeek=throwState.coverSeek;prepareSpContext(app,bot,.01f,false);
     check(throwState.coverSeek>0&&throwState.coverSeek<heldSeek,"new cover seek cancelled on the next policy frame");
     bot.input={};bot.velocity={};bot.yaw=0;
    }
    applySpCoverIntent(app,bot,.01f);check(throwState.cover>0,"standing beside cover did not finish arrival validation");
    updateSpContextPose(app,bot,.01f);check(throwState.overlay<app.botActorScene->animations.size(),"cover idle overlay absent");
    const auto idleOverlay=throwState.overlay;const auto entryWeight=throwState.weight;
    bot.input.fire=true;bot.firePoseTime=.1f;throwState.coverElapsed=.4f;updateSpContextPose(app,bot,.01f);
    check(throwState.previousOverlay==idleOverlay&&throwState.transitionElapsed<.16f&&throwState.weight>=entryWeight,"cover fire reset envelope instead of crossfading outgoing idle");
    const auto active=std::find_if(app.botSpContextClips.begin(),app.botSpContextClips.end(),[&](const auto& entry){return entry.animation==throwState.overlay;});
    check(active!=app.botSpContextClips.end()&&active->descriptor.kind==ContextKind::CoverFire,"cover fire context absent");
    app.loadedMap->shotBlocked=true;check(!spContextShotHits(app,bot),"cover damaged player through obstructed shot ray");
    app.loadedMap->covered=false;bot.input={};bot.firePoseTime=0;prepareSpContext(app,bot,.31f,true);check(throwState.cover==0,"removed cover retained its behavior episode");
    app.loadedMap->covered=true;app.loadedMap->height=gameplay::iw::worldUnits(36);throwState={};throwState.clock.elapsed=20;bot.stance=scene::Stance::Stand;
    prepareSpContext(app,bot,.01f,true);applySpCoverIntent(app,bot,.01f);check(throwState.cover>0&&throwState.coverHeightIw<44,"low cover did not select crouched episode");
    bot.stance=throwState.coverHeightIw<44?scene::Stance::Crouch:scene::Stance::Stand;
    updateSpContextPose(app,bot,.01f);const auto crouched=std::find_if(app.botSpContextClips.begin(),app.botSpContextClips.end(),[&](const auto& entry){return entry.animation==throwState.overlay;});
    check(crouched!=app.botSpContextClips.end()&&crouched->descriptor.name=="covercrouch_hide_idle","low cover chose standing or rejected crouch clip");
    spContextShotHits(app,bot);check(app.loadedMap->lastShotHeight<120&&app.loadedMap->lastShotHeight>app.loadedMap->height,"crouch cover shot used standing-height LOS");
    const auto lowCoverClip=throwState.coverClip;std::erase_if(app.botSpContextClips,[](const auto& e){return e.descriptor.kind==ContextKind::CoverIdle||e.descriptor.kind==ContextKind::CoverFire;});updateSpContextPose(app,bot,.01f);
    check(throwState.overlay==lowCoverClip,"committed low cover fell back to native MP idle when optional selection unavailable");
    app.botSpContextClips.clear();for(std::size_t i=0;i<std::size(kContextDescriptors);++i)app.botSpContextClips.push_back({i,kContextDescriptors[i]});
    throwState.cover=0;bot.targetVisible=false;const float exitWeight=throwState.weight;updateSpContextPose(app,bot,.01f);
    check(throwState.weight>0&&throwState.weight<exitWeight,"cover exit snapped instead of fading out");
    app.botSpContextClips.clear();for(std::size_t i=0;i<std::size(kContextDescriptors);++i)app.botSpContextClips.push_back({i,kContextDescriptors[i]});
    bot.stance=scene::Stance::Stand;throwState={};app.loadedMap=ContextMap{};app.loadedMap->nearbyOnly=true;
    check(beginSpCoverSeek(app,bot)&&throwState.coverSeek>0&&horizontalDistance(bot.position,throwState.coverGoal)>90,"nearby cover was not sought from outside local cover");
    const auto startPosition=bot.position;applySpCoverIntent(app,bot,.01f);
    check(bot.spWantsMove&&scene::length(bot.spMoveGoal-throwState.coverGoal)<.01f&&scene::length(bot.position-startPosition)<.01f&&throwState.cover==0,"seek did not set real movement goal or teleported/posed before arrival");
    applySpCoverIntent(app,bot,1.6f);check(throwState.coverSeek==0&&!bot.spWantsMove,"stalled cover intent did not abort");
    check(beginSpCoverSeek(app,bot),"second bounded cover seek failed");app.botSpCoverChance=0;prepareSpContext(app,bot,.01f,true);
    check(throwState.coverSeek==0&&!bot.spWantsMove&&!beginSpCoverSeek(app,bot),"chance zero retained or started cover seeking");
    app.botSpCoverChance=1;app.loadedMap->covered=false;app.loadedMap->groundQueries=0;
    app.loadedMap->surfaceQueries=0;
    check(!beginSpCoverSeek(app,bot)&&app.loadedMap->groundQueries<=34&&app.loadedMap->surfaceQueries<=25,"empty map cover query escaped fixed candidate budget");
    app.loadedMap->covered=true;app.loadedMap->capsuleBlocked=true;
    check(!beginSpCoverSeek(app,bot),"embedded cover destination was accepted");
    app.loadedMap->capsuleBlocked=false;check(beginSpCoverSeek(app,bot),"valid seek failed after obstruction removed");
    prepareSpContext(app,bot,5.1f,true);check(throwState.coverSeek==0,"cover seek timeout did not abort");
    throwState={};app.loadedMap=ContextMap{};app.botSpCoverChance=1;
    bot.input.fire=true;bot.firePoseTime=.2f;bot.velocity={};throwState.clock.elapsed=1;
    prepareSpContext(app,bot,.01f,false);
    check(throwState.coverSeek>0,"occluded target or combat fire pose starved nearby cover opportunity");
    throwState={};app.botSpCoverChance=0;app.botSpGrenadeChance=1;app.botSpWoundedChance=0;
    bot.input={};bot.firePoseTime=0;bot.velocity={150,0,0};throwState.clock.elapsed=1;
    check(prepareSpContext(app,bot,.01f,false)&&throwState.throwTime>0,"moving patrol could not reserve a cosmetic throw stop");
    updateSpContextPose(app,bot,.01f);
    check(throwState.weight==0&&throwState.frame==0,"stationary throw played before normal physics stopped movement");
    bot.velocity={};updateSpContextPose(app,bot,.01f);
    check(throwState.weight>0&&throwState.frame>0,"reserved throw did not animate after settling");
    throwState={};app.botSpGrenadeChance=0;app.botSpWoundedChance=1;throwState.clock.elapsed=1;
    prepareSpContext(app,bot,.01f,false);
    check(throwState.wounded>0&&bot.input.forward>0,"stationary bot did not reserve actual wounded forward movement");
    throwState={};app.botSpGrenadeChance=1;app.botSpWoundedChance=app.botSpCoverChance=0;
    for(auto& clip:app.botActorScene->animations)clip.durationFrames=150;
    bot.velocity={150,0,0};throwState.clock.elapsed=1;
    prepareSpContext(app,bot,.01f,true);const float authoredDuration=throwState.throwTime;
    for(int i=0;i<100;++i)prepareSpContext(app,bot,.01f,true);
    check(authoredDuration==5.f&&throwState.throwTime==authoredDuration,"settling consumed or truncated authored five-second throw");
    bot.velocity={};for(int i=0;i<400;++i)prepareSpContext(app,bot,.01f,true);
    check(throwState.throwTime>.9f,"authored throw ended before its full duration");
    throwState={};app.botSpGrenadeChance=app.botSpWoundedChance=app.botSpCoverChance=1;
    app.loadedMap=ContextMap{};bot.input={};bot.firePoseTime=0;bot.position={};bot.yaw=0;
    unsigned throws{},wounds{},covers{},movingGrants{},stationaryGrants{};
    for(int i=0;i<2400;++i){
        const bool wasThrow=throwState.throwTime>0,wasWound=throwState.wounded>0,wasCover=throwState.coverSeek>0||throwState.cover>0;
        prepareSpContext(app,bot,.05f,true);
        throws+=!wasThrow&&throwState.throwTime>0;wounds+=!wasWound&&throwState.wounded>0;covers+=!wasCover&&throwState.coverSeek>0;
        movingGrants+=throwState.varietyGrant==1;stationaryGrants+=throwState.varietyGrant==2;
        applySpCoverIntent(app,bot,.05f);updateSpContextPose(app,bot,.05f);
    }
    check(throws>1&&wounds>1&&covers>1&&movingGrants>1&&stationaryGrants>1,"chance-one families or contextual categories starved under visible combat");
    check(bot.health==100&&bot.ammo==6,"cosmetic scheduler spent health or ammo");
    throwState={};bot.position={};bot.yaw=0;bot.velocity={};bot.input={};app.loadedMap=ContextMap{};
    throwState.coverSeek=5;throwState.coverGoal={31.6f,0,0};throwState.coverBestDistance=31.6f;throwState.coverCheck=1;
    bot.yaw=1.f;restoreSpContextYaw(app,bot,0.f,.033f);
    check(bot.yaw==0,"ordinary policy yaw overrode precise cover approach");
    applySpCoverIntent(app,bot,.033f);
    check(bot.input.forward>=.30f&&throwState.coverSeek>0&&throwState.cover==0,"final cover approach stopped at generic route tolerance");
    check(bot.position.x==0&&bot.velocity.x==0,"final cover approach snapped position or velocity");
    throwState.coverSeek=0;throwState.cover=2;bot.yaw=1.f;restoreSpContextYaw(app,bot,0.f,.033f);
    check(bot.yaw==0,"ordinary policy yaw rotated away from anchored cover");
    throwState={};
    bot.mantling=true;bot.grounded=false;bot.mantleStart={};bot.mantleEnd={100,0,gameplay::iw::worldUnits(48)};
    const auto mantle=spContextBaseAnimation(app,bot);check(mantle.has_value(),"validated 48-inch mantle failed context selection");
    if(mantle){const auto entry=std::find_if(app.botSpContextClips.begin(),app.botSpContextClips.end(),[&](const auto& e){return e.animation==*mantle;});check(entry->descriptor.name=="ai_mantle_on_48","mantle height mismatch");}
    if(!failures)std::cout<<"Actual SP runtime: cosmetic throw/no-ammo, delta-zero pose, ambient wounded expiry, local cover/fire LOS and mantle selection PASS.\n";
    return failures?1:0;
}
