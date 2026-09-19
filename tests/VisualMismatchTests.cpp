#include "app/WorldActionBlend.h"
#include "app/ReplayVisibility.h"
#include "take/PoseInterpolation.h"
#include "weapon/WeaponProfile.h"
#include <iostream>

int main(){
    int failures=0;const auto check=[&](bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';++failures;}};
    scene::CastScene rig;rig.skeleton.bones.resize(3);
    rig.skeleton.bones[1].parent=0;rig.skeleton.bones[2].parent=1;
    rig.skeleton.bones[1].restLocal.position={100,0,0};rig.skeleton.bones[2].restLocal.position={60,0,0};
    auto a=rig.globalPose({rig.skeleton.bones[0].restLocal,rig.skeleton.bones[1].restLocal,rig.skeleton.bones[2].restLocal});
    const auto rotation=scene::rotation(scene::fromAxisAngle({0,0,1},scene::kPi/2));
    auto b=a;for(auto& m:b)m=rotation*m;
    take::Take take;take.boneCount=3;take.botCount=1;take.botBoneCount=3;take.worldBoneCount=3;
    take::Sample low,high;low.time=0;high.time=1;low.pose=a;high.pose=b;low.worldActor.pose=a;high.worldActor.pose=b;
    take::RecordedActorState bot;bot.id=5;bot.pose=a;low.bots={bot};bot.pose=b;high.bots={bot};take.samples={low,high};
    for(float t:{.75f,.25f,.5f,.001f,.999f}){
        const auto sample=take.interpolatedSample(t,{&rig.skeleton,&rig.skeleton,&rig.skeleton});
        for(const auto* pose:{&sample.pose,&sample.worldActor.pose,&sample.bots[0].pose}){
            const auto p0=scene::transformPoint((*pose)[0],{}),p1=scene::transformPoint((*pose)[1],{}),p2=scene::transformPoint((*pose)[2],{});
            check(std::abs(scene::length(p1-p0)-100)<.002f&&std::abs(scene::length(p2-p1)-60)<.002f,"fractional replay must preserve articulated lengths on all actors");
        }
    }
    check(take.interpolatedSample(0,{&rig.skeleton}).pose[1].v==a[1].v&&take.interpolatedSample(1,{&rig.skeleton}).pose[1].v==b[1].v,"exact recorded samples unchanged");
    auto reverseRig=rig.skeleton;std::swap(reverseRig.bones[0],reverseRig.bones[2]);reverseRig.bones[0].parent=1;reverseRig.bones[1].parent=2;
    auto reverseA=a,reverseB=b;std::swap(reverseA[0],reverseA[2]);std::swap(reverseB[0],reverseB[2]);
    auto reversed=take::interpolatePose(reverseA,reverseB,.5f,&reverseRig);
    check(std::abs(scene::length(scene::transformPoint(reversed[0],{})-scene::transformPoint(reversed[1],{}))-60)<.002f,"non-topological hierarchies work");
    reverseRig.bones[2].parent=0;auto cyclic=take::interpolatePose(reverseA,reverseB,.5f,&reverseRig);
    for(const auto& m:cyclic)for(auto v:m.v)check(std::isfinite(v),"malformed hierarchy must remain finite");
    take.samples[0].bots[0].alive=false;take.samples[1].bots[0].alive=true;
    take.samples[1].bots[0].pose[0]=scene::translation({1000,0,0});
    for(float t:{.25f,.75f,.999f,.25f}){auto s=take.interpolatedSample(t);check(!s.bots[0].alive&&s.bots[0].pose[0].v[12]==0,"corpse stays put before respawn, including reverse seeking");}
    check(take.interpolatedSample(1).bots[0].alive&&take.interpolatedSample(1).bots[0].pose[0].v[12]==1000,"respawn switches at the recorded boundary");
    take.samples[0].bots[0].alive=true;take.samples[1].bots[0].alive=false;
    check(take.interpolatedSample(.5f).bots[0].pose[0].v[12]==500,"death transition remains interpolated");

    for(bool hide:{false,true})for(bool sniper:{false,true})for(bool equipment:{false,true}){
        const auto bits=cadence::actorVisibility(equipment,true,1,hide,sniper);
        check(take::validVisibility(bits)&&bool(bits&take::HideWeapon)==equipment&&bool(bits&take::HideViewmodel)==hide&&bool(bits&take::ScopeOverlay)==(hide&&sniper),"ADS visibility preserves setting independently of equipment and sniper class");
    }
    check(!(cadence::actorVisibility(false,true,.5f,true,true)&take::HideViewmodel),"ADS transition stays visible");
    check(!take::validVisibility(16)&&!take::validVisibility(take::HideWeapon)&&!take::validVisibility(take::VisibilityKnown|take::ScopeOverlay),"malformed visibility flags rejected");

    scene::Animation mg,rifle;scene::classifyAnimationName("pt_reload_stand_mg.cast",mg);scene::classifyAnimationName("pt_reload_stand_rifle.cast",rifle);
    mg.tracks.push_back({});rifle.tracks.push_back({});rig.animations={mg,rifle};
    scene::AnimationQuery query;query.domain=scene::AnimationDomain::PlayerTorso;query.action=scene::ActionRole::Reload;query.weapon=scene::WeaponClass::Rifle;query.stance=scene::Stance::Stand;
    check(mg.weapon==scene::WeaponClass::LMG&&scene::findBestAnimation(rig,query)==1,"rifle reload must not choose MW3 MG clip");
    query.weapon=scene::WeaponClass::LMG;check(scene::findBestAnimation(rig,query)==0,"LMG reload remains available to LMGs");
    for(const char* name:{"weapon_usp45_iw5_LOD0","weapon_desert_eagle_iw5_LOD0"})check(weapon::inferArchetype(name)==weapon::Archetype::Pistol,"world pistols without pistol in filename use handgun reference");

    scene::Animation action;action.durationFrames=30;scene::Track track;track.boneIndex=1;track.property=scene::TrackProperty::TranslationX;track.frames={0,30};track.scalarValues={150,150};action.tracks.push_back(track);rig.animations={action,action};
    rig.animations[1].tracks[0].scalarValues={200,200};
    cadence::WorldActionBlend blend;
    const auto step=[&](std::optional<std::size_t> clip,float frame,float dt,float root,float child){
        std::vector<scene::Transform> pose(3);pose[0].position.x=root;pose[1].position.x=100;pose[2].position.x=child;
        blend.apply(rig,clip,frame,dt,.1f,pose);return pose;
    };
    step({},0,0,0,60);
    check(step(0,0,.016f,1,61)[1].position.x==100,"action entry starts at current pose");
    auto middle=step(0,1,.05f,2,62);
    check(std::abs(middle[1].position.x-125)<.001f&&middle[0].position.x==2&&middle[2].position.x==62,"local blend leaves root and unowned locomotion live");
    check(step(0,2,.05f,3,63)[1].position.x==150,"action reaches authored pose");
    check(step(1,0,.016f,4,64)[1].position.x==150,"interrupt starts from current action pose");
    check(step(1,1,.1f,5,65)[1].position.x==200,"interrupt reaches new action");
    check(step({},0,.016f,6,66)[1].position.x==200,"action exit does not snap");
    check(std::abs(step({},0,.05f,7,67)[1].position.x-150)<.001f,"action exit blends back to moving base");
    check(step({},0,.05f,8,68)[1].position.x==100,"action exit finishes");
    step(0,0,0,0,60);step(0,10,.1f,0,60);
    const auto repeat=step(0,0,.016f,0,60);check(repeat[1].position.x==150,"repeated same-clip action starts from current pose");
    if(!failures)std::cout<<"Visual mismatch regression tests passed.\n";
    return failures?1:0;
}
