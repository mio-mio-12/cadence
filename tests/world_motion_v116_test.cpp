#include "scene/CastScene.h"
#include "assets/CharacterParts.h"
#include <algorithm>
#include <cctype>
#include <cassert>
#include <iostream>
std::string lowerText(std::string text){for(auto& c:text)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return text;}
#include "../src/app/WorldMotionV116.inc"
int main(){
    {
        scene::CastScene s;
        for(const char* name:{"pb_grenade_run_f.cast","pb_rifle_crouch_walk_lowready_f.cast","pb_rifle_stand_idle.cast"}){
            scene::Animation a;a.sourceName=name;scene::classifyAnimationName(name,a);a.tracks.emplace_back();s.animations.push_back(a);
        }
        scene::Animation fallback;fallback.domain=scene::AnimationDomain::PlayerBody;fallback.motion=scene::MotionRole::Run;fallback.stance=scene::Stance::Stand;fallback.weapon=scene::WeaponClass::Rifle;fallback.direction=scene::Direction::Forward;
        assert(needsWorldMovementFallback(s,fallback,3)); // Grenade carry is not a rifle substitute.
        fallback.stance=scene::Stance::Crouch;
        assert(!needsWorldMovementFallback(s,fallback,3)); // Keep native walk even for a run query.
        fallback.stance=scene::Stance::Stand;fallback.tracks.emplace_back();s.animations.push_back(fallback);
        assert(needsWorldMovementFallback(s,fallback,3)); // Imported variants cannot hide remaining holes.
        assert(!needsWorldMovementFallback(s,fallback,4));
        for(auto action:{scene::ActionRole::Fire,scene::ActionRole::Reload}){fallback.action=action;assert(!needsWorldMovementFallback(s,fallback,3));}
        fallback.action=scene::ActionRole::None;fallback.motion=scene::MotionRole::Idle;assert(!needsWorldMovementFallback(s,fallback,3));
    }
    {
        scene::CastScene s;
        for(const auto* name:{"mp_stand_idle_twitch_b.cast","mp_stand_idle.cast"}){
            scene::Animation a;scene::classifyAnimationName(name,a);a.sourceName=name;a.tracks.emplace_back();s.animations.push_back(a);
        }
        s.animations[0].looping=false;s.animations[1].looping=true;
        scene::AnimationQuery q;q.domain=scene::AnimationDomain::PlayerBody;q.motion=scene::MotionRole::Idle;q.stance=scene::Stance::Stand;q.weapon=scene::WeaponClass::Rifle;q.direction=scene::Direction::Backward;
        assert(worldClipDirection(s.animations[0])==scene::Direction::Any);
        assert(selectGroundWorldLocomotion(s,q)==1);
        for(const char* suffix:{"f","b","l","r"})for(const char* ending:{".cast","_variant.cast"}){
            scene::Animation a;a.sourceName=std::string("mp_stand_idle_twitch_")+suffix+ending;scene::classifyAnimationName(a.sourceName,a);
            assert(worldClipDirection(a)==scene::Direction::Any);
        }
        for(const char* name:{"mp_stand_idle_backward.cast","pb_stand_walk_b.cast","pb_stand_run_b.cast","pb_stand_run_b_2_idle.cast"}){
            scene::Animation a;a.sourceName=name;scene::classifyAnimationName(name,a);
            assert(worldClipDirection(a)==scene::Direction::Backward);
        }
    }
    {
        scene::CastScene s;
        for(const auto* name:{"mp_female_assault_sprint.cast","pb_stand_shoot_walk_forward.cast","pt_rifle_fire.cast"}){
            scene::Animation a;scene::classifyAnimationName(name,a);a.sourceName=name;a.tracks.emplace_back();s.animations.push_back(a);
        }
        assert(s.animations[1].action==scene::ActionRole::None&&s.animations[1].ads);
        assert(s.animations[2].action==scene::ActionRole::Fire);
        scene::AnimationQuery q;q.domain=scene::AnimationDomain::PlayerBody;q.motion=scene::MotionRole::Walk;q.stance=scene::Stance::Stand;q.weapon=scene::WeaponClass::Rifle;q.direction=scene::Direction::Forward;
        assert(selectGroundWorldLocomotion(s,q)==1);
    }
    {
        scene::CastScene s;
        for(const auto* name:{"mp_stand_flash_reaction_b.cast","pb_stand_ads.cast"}){scene::Animation a;scene::classifyAnimationName(name,a);a.sourceName=name;a.tracks.emplace_back();s.animations.push_back(a);}
        assert(s.animations[0].contextual);
        scene::AnimationQuery q;q.domain=scene::AnimationDomain::PlayerBody;q.motion=scene::MotionRole::Idle;q.stance=scene::Stance::Stand;q.weapon=scene::WeaponClass::Rifle;q.direction=scene::Direction::Backward;
        assert(selectGroundWorldLocomotion(s,q)==1);
        scene::Animation view;scene::classifyAnimationName("viewmodel_flash_reaction.cast",view);assert(!view.contextual);
    }
    {
        scene::CastScene s;
        for(const auto* name:{"mp_akimbo_crouch_walk_f.cast","pb_crouch_hold_run.cast"}){
            scene::Animation a;scene::classifyAnimationName(name,a);a.sourceName=name;a.tracks.emplace_back();s.animations.push_back(a);
        }
        assert(!hasCrouchedLongGunLocomotion(s,scene::Direction::Forward));
        scene::Animation native;scene::classifyAnimationName("pb_crouch_run_forward.cast",native);
        native.sourceName="pb_crouch_run_forward.cast";native.tracks.emplace_back();s.animations.push_back(native);
        assert(hasCrouchedLongGunLocomotion(s,scene::Direction::Forward));
        assert(!hasCrouchedLongGunLocomotion(s,scene::Direction::Backward));
        scene::AnimationQuery q;q.domain=scene::AnimationDomain::PlayerBody;q.stance=scene::Stance::Crouch;
        q.weapon=scene::WeaponClass::Rifle;q.motion=scene::MotionRole::Walk;q.direction=scene::Direction::Forward;
        assert(selectGroundWorldLocomotion(s,q)==2);
        s.animations.back().motion=scene::MotionRole::Walk;
        assert(hasCrouchedLongGunLocomotion(s,scene::Direction::Forward));
        s.animations.back().contextual=true;assert(!hasCrouchedLongGunLocomotion(s,scene::Direction::Forward));
    }
    {
        scene::CastScene s;
        for(const auto name:{"pb_rifle_stand_shuffle_f.cast","pb_walk_forward.cast"}){scene::Animation c;scene::classifyAnimationName(name,c);c.sourceName=name;c.sourceGame="bo2";c.tracks.emplace_back();s.animations.push_back(c);}
        scene::AnimationQuery q;q.motion=scene::MotionRole::Walk;q.weapon=scene::WeaponClass::Rifle;q.stance=scene::Stance::Stand;q.direction=scene::Direction::Forward;q.ads=true;
        assert(selectGroundWorldLocomotion(s,q)==1);
    }
    assert(std::abs(worldMovementFacing(0,{100,100,0},scene::Direction::Forward)-scene::kPi*.25f)<.0001f);
    assert(std::abs(worldMovementFacing(0,{100,-100,0},scene::Direction::Forward)+scene::kPi*.25f)<.0001f);
    assert(std::abs(worldMovementFacing(0,{0,-100,0},scene::Direction::Right))<.0001f);
    assert(std::abs(worldMovementFacing(0,{-100,0,0},scene::Direction::Backward))<.0001f);
    assert(worldMovementFacing(.7f,{0,0,0},scene::Direction::Forward)==.7f);
    assert(std::abs(worldMovementFacing(0,{-100,0,0},scene::Direction::Forward))<.0001f);
    assert(std::abs(worldMovementFacing(0,{-100,100,0},scene::Direction::BackwardLeft))<.0001f);
    for(const auto authored:{scene::Direction::Any,scene::Direction::Forward}){
        assert(std::abs(worldPresentationMovementFacing(0,{0,100,0},authored,scene::MotionRole::Jump,scene::Direction::Left))<.0001f);
        assert(std::abs(worldPresentationMovementFacing(0,{0,-100,0},authored,scene::MotionRole::Jump,scene::Direction::Right))<.0001f);
        assert(std::abs(worldPresentationMovementFacing(0,{100,100,0},authored,scene::MotionRole::Jump,scene::Direction::Forward)-scene::kPi*.25f)<.0001f);
        assert(std::abs(worldPresentationMovementFacing(0,{-100,0,0},authored,scene::MotionRole::Jump,scene::Direction::Backward))<.0001f);
        assert(worldPresentationMovementFacing(.7f,{0,0,0},authored,scene::MotionRole::Jump,scene::Direction::Any)==.7f);
    }
    for(const auto motion:{scene::MotionRole::Walk,scene::MotionRole::Run,scene::MotionRole::Sprint,scene::MotionRole::Crawl,scene::MotionRole::Slide,scene::MotionRole::Climb})
        assert(worldPresentationMovementFacing(0,{0,100,0},scene::Direction::Forward,motion,scene::Direction::Left)==worldMovementFacing(0,{0,100,0},scene::Direction::Forward));
    const auto facing=smoothWorldFacingOffset(0,scene::kPi*.5f,1.f/60);
    assert(facing>0&&facing<scene::kPi*.5f);
    assert(smoothWorldFacingOffset(.3f,1.f,0)==.3f);
    float offset=0;for(int i=0;i<60;++i)offset=smoothWorldFacingOffset(offset,scene::kPi*.5f,1.f/60);
    assert(std::abs(offset-scene::kPi*.5f)<.001f);
    assert(!assets::character::headCompatible("ghosts","mp_body_elite_pmc_assault_b_arctic_LOD0","head_juggernaut_light_black_LOD0"));
    assert(assets::character::headCompatible("ghosts","mp_body_elite_pmc_assault_b_arctic_LOD0","head_mp_head_a_LOD0"));
    scene::CastScene actor;
    const auto add=[&](const char* name,scene::MotionRole motion){scene::Animation a;a.sourceName=name;a.sourceGame="ghosts";a.domain=scene::AnimationDomain::PlayerBody;a.motion=motion;a.tracks.emplace_back();actor.animations.push_back(a);};
    add("mp_mantle_32_over.cast",scene::MotionRole::Climb);
    add("mp_mantle_56_up.cast",scene::MotionRole::Climb);
    add("pb_rifle_mantle_over_low_l_36.cast",scene::MotionRole::Climb);
    add("pb_rifle_mantle_over_low_r_36.cast",scene::MotionRole::Climb);
    assert(selectWorldMantle(actor,56,false,scene::Direction::Forward,"ghosts",scene::WeaponClass::Any)==1);
    assert(selectWorldMantle(actor,36,true,scene::Direction::Left,"ghosts",scene::WeaponClass::Any)==2);
    assert(selectWorldMantle(actor,36,true,scene::Direction::Right,"ghosts",scene::WeaponClass::Any)==3);
    add("pb_rifle_run_slide_land_r.cast",scene::MotionRole::Slide);
    assert(selectDirectionalWorldClip(actor,scene::MotionRole::Slide,scene::Direction::Right,"ghosts")==4);
    assert(!selectDirectionalWorldClip(actor,scene::MotionRole::Slide,scene::Direction::Left,"ghosts"));
    add("mp_slide_akimbo.cast",scene::MotionRole::Slide);
    add("mp_slide.cast",scene::MotionRole::Slide);
    assert(selectDirectionalWorldClip(actor,scene::MotionRole::Slide,scene::Direction::Forward,"ghosts",scene::WeaponClass::Rifle)==6);
    add("pb_slide_jugmaniac_2crouch.cast",scene::MotionRole::Slide);actor.animations.back().sourceGame="aw";
    assert(selectDirectionalWorldClip(actor,scene::MotionRole::Slide,scene::Direction::Forward,"aw",scene::WeaponClass::Rifle)==6);
    scene::CastScene empty;add("pb_ladder_climb.cast",scene::MotionRole::Climb);
    assert(!selectWorldMantle(empty,32,true,scene::Direction::Forward,"ghosts",scene::WeaponClass::Any));
    actor.animations.clear();
    add("pb_combatrun_forward_akimbo.cast",scene::MotionRole::Run);
    add("pb_combatrun_back_loop_assault.cast",scene::MotionRole::Run);
    add("pb_combatrun_forward_loop_smg.cast",scene::MotionRole::Run);
    for(auto& a:actor.animations)a.weapon=scene::WeaponClass::Rifle;
    scene::AnimationQuery q;q.motion=scene::MotionRole::Run;q.weapon=scene::WeaponClass::Rifle;q.stance=scene::Stance::Stand;q.direction=scene::Direction::Forward;q.preferredGame="ghosts";
    assert(selectCanonicalLongGunLocomotion(actor,q)==2);
    add("pb_combatrun_forward_loop_assault.cast",scene::MotionRole::Run);actor.animations.back().weapon=scene::WeaponClass::Rifle;
    assert(selectCanonicalLongGunLocomotion(actor,q)==3);
    add("pb_combatrun_forward_loop.cast",scene::MotionRole::Run);actor.animations.back().weapon=scene::WeaponClass::Rifle;actor.animations.back().sourceGame="bo2";
    assert(selectCanonicalLongGunLocomotion(actor,q)==3);
    q.forceT6Locomotion=true;assert(selectCanonicalLongGunLocomotion(actor,q)==4);
    scene::CastScene boosts;
    scene::Animation generic;generic.sourceName="pb_standjump_boost_takeoff.cast";generic.domain=scene::AnimationDomain::PlayerBody;generic.tracks.emplace_back();boosts.animations.push_back(generic);
    assert(selectWorldBoost(boosts,scene::WeaponClass::Rifle)==0);
    assert(!selectWorldBoost(boosts,scene::WeaponClass::Knife));
    auto knife=generic;knife.sourceName="pb_standjump_boost_takeoff_knife.cast";boosts.animations.push_back(knife);
    assert(selectWorldBoost(boosts,scene::WeaponClass::Knife)==1);
    assert(selectWorldBoost(boosts,scene::WeaponClass::Sniper)==0);
    assert(isWorldBoostClip(knife)&&isWorldBoostClip(generic));
    boosts.animations[1].domain=scene::AnimationDomain::ViewModel;
    assert(!selectWorldBoost(boosts,scene::WeaponClass::Knife));
    std::cout<<"World motion selection and knife boost checks passed\n";
    actor.animations.clear();
    add("pb_sprint_forward_pistol.cast",scene::MotionRole::Sprint);actor.animations.back().weapon=scene::WeaponClass::Pistol;
    assert(!selectDirectionalWorldClip(actor,scene::MotionRole::Sprint,scene::Direction::Forward,"ghosts",scene::WeaponClass::Sniper));
    add("pb_sprint_forward_rifle.cast",scene::MotionRole::Sprint);actor.animations.back().weapon=scene::WeaponClass::Rifle;
    assert(selectDirectionalWorldClip(actor,scene::MotionRole::Sprint,scene::Direction::Forward,"ghosts",scene::WeaponClass::Sniper)==1);
    assert(!selectDirectionalWorldClip(actor,scene::MotionRole::Sprint,scene::Direction::Forward,"ghosts",scene::WeaponClass::Knife));
    actor.animations.clear();
    for(const auto* name:{"pb_stand_alert.cast","pb_prone_crawl_back.cast","pb_combatrun_back_loop.cast","pb_combatrun_forward_loop.cast","pb_crouch_walk_back_pistol.cast"}){
        scene::Animation a;scene::classifyAnimationName(name,a);a.tracks.push_back({});a.sourceGame="mw";actor.animations.push_back(a);
    }
    q={};q.domain=scene::AnimationDomain::PlayerBody;q.motion=scene::MotionRole::Run;q.stance=scene::Stance::Stand;q.weapon=scene::WeaponClass::Pistol;q.direction=scene::Direction::Backward;q.preferredGame="mw";
    assert(selectGroundWorldLocomotion(actor,q)==2); // stance wins over weapon match
    q.ads=true;q.motion=scene::MotionRole::Walk;
    assert(selectGroundWorldLocomotion(actor,q)==2); // ADS never freezes legs
    q.stance=scene::Stance::Crouch;assert(selectGroundWorldLocomotion(actor,q)==4);
    q.stance=scene::Stance::Prone;q.motion=scene::MotionRole::Crawl;assert(selectGroundWorldLocomotion(actor,q)==1);
    q.stance=scene::Stance::Stand;q.motion=scene::MotionRole::Idle;assert(selectGroundWorldLocomotion(actor,q)==0);
    actor.animations.clear();
    for(const auto* name:{"mp_assault_sprint.cast","pb_combatrun_forward_loop_lmg.cast","pb_walk_forward.cast"}){
        scene::Animation a;scene::classifyAnimationName(name,a);a.tracks.push_back({});a.sourceGame="ghosts";a.direction=scene::Direction::Forward;actor.animations.push_back(a);
    }
    q={};q.domain=scene::AnimationDomain::PlayerBody;q.stance=scene::Stance::Stand;q.weapon=scene::WeaponClass::Rifle;q.direction=scene::Direction::Forward;
    q.motion=scene::MotionRole::Run;assert(selectGroundWorldLocomotion(actor,q)==1);
    q.motion=scene::MotionRole::Walk;assert(selectGroundWorldLocomotion(actor,q)==2);
    q.motion=scene::MotionRole::Sprint;assert(selectGroundWorldLocomotion(actor,q)==0);
}
