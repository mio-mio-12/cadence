#include "scene/CastScene.h"
#include "assets/CharacterParts.h"
#include <algorithm>
#include <cctype>
#include <cassert>
#include <iostream>
std::string lowerText(std::string text){for(auto& c:text)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return text;}
#include "../src/app/WorldMotionV116.inc"
int main(){
    assert(std::abs(worldMovementFacing(0,{100,100,0},scene::Direction::Forward)-scene::kPi*.25f)<.0001f);
    assert(std::abs(worldMovementFacing(0,{100,-100,0},scene::Direction::Forward)+scene::kPi*.25f)<.0001f);
    assert(std::abs(worldMovementFacing(0,{0,-100,0},scene::Direction::Right))<.0001f);
    assert(std::abs(worldMovementFacing(0,{-100,0,0},scene::Direction::Backward))<.0001f);
    assert(worldMovementFacing(.7f,{0,0,0},scene::Direction::Forward)==.7f);
    assert(std::abs(worldMovementFacing(0,{-100,0,0},scene::Direction::Forward))<.0001f);
    assert(std::abs(worldMovementFacing(0,{-100,100,0},scene::Direction::BackwardLeft))<.0001f);
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
}
