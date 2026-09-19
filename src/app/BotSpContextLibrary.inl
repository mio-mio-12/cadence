#pragma once

#include "gameplay/BotSpVariety.h"
#include "scene/CastScene.h"
#include <array>
#include <cmath>
#include <optional>
#include <string_view>
#include <vector>

// Explicit names inspected in the BO2_SP rip. Presence is not visual approval:
// root-motion/pose validation still belongs to the loader and runtime anchors.
namespace cadence::sp {
// Match the existing CastScene generic rifle-pose fallback, without treating
// shields, dual pistols, heavy/miniguns or props as ordinary two-hand long guns.
inline bool supportsSpLongGun(scene::WeaponClass weapon){
    using W=scene::WeaponClass;
    return weapon==W::Rifle||weapon==W::Automatic||weapon==W::Sniper||weapon==W::Shotgun||weapon==W::M1216||weapon==W::G11||weapon==W::LMG||weapon==W::Crossbow;
}

enum class ContextKind { CoverIdle, CoverFire, Death, Throw, StandAim, StandFire, TraversalMantle, Jump, WoundedWalk, WoundedIdle };
enum class ContextStance { Stand, Crouch, Any };
struct ContextDescriptor {
    std::string_view name;
    ContextKind kind;
    ContextStance stance{ContextStance::Stand};
    bool looping{},moving{},rifleOnly{true};
    float heightIw{},pitchDegrees{};
    bool over{};
};

inline constexpr auto kContextDescriptors=std::to_array<ContextDescriptor>({
    {"coverstand_hide_idle",ContextKind::CoverIdle,ContextStance::Stand,true,false,true,56},
    {"covercrouch_hide_idle",ContextKind::CoverIdle,ContextStance::Crouch,true,false,true,36},
    {"coverstand_blindfire_1",ContextKind::CoverFire,ContextStance::Stand,false,false,true,56},
    {"coverstand_blindfire_2",ContextKind::CoverFire,ContextStance::Stand,false,false,true,56},
    {"covercrouch_blindfire_1",ContextKind::CoverFire,ContextStance::Crouch,false,false,true,36},
    {"covercrouch_blindfire_2",ContextKind::CoverFire,ContextStance::Crouch,false,false,true,36},
    {"ai_death_collapse_in_place",ContextKind::Death,ContextStance::Stand,false,false,false},
    {"ai_death_flatonback",ContextKind::Death,ContextStance::Stand,false,false,false},
    {"ai_death_stumblefall",ContextKind::Death,ContextStance::Stand,false,true,false},
    {"ai_death_fallforward",ContextKind::Death,ContextStance::Stand,false,true,false},
    {"ai_death_fallforward_b",ContextKind::Death,ContextStance::Stand,false,true,false},
    {"stand_grenade_throw",ContextKind::Throw},
    {"crouch_grenade_throw",ContextKind::Throw,ContextStance::Crouch},
    {"ai_stand_exposed_grenade_throwa",ContextKind::Throw},
    {"stand_aim_up",ContextKind::StandAim,ContextStance::Stand,true,false,true,0,45},
    {"stand_aim_straight",ContextKind::StandAim,ContextStance::Stand,true,false,true,0,0},
    {"stand_aim_down",ContextKind::StandAim,ContextStance::Stand,true,false,true,0,-45},
    {"stand_shoot_auto_up",ContextKind::StandFire,ContextStance::Stand,true,false,true,0,45},
    {"stand_shoot_auto_straight",ContextKind::StandFire,ContextStance::Stand,true,false,true,0,0},
    {"stand_shoot_auto_down",ContextKind::StandFire,ContextStance::Stand,true,false,true,0,-45},
    {"ai_mantle_on_36",ContextKind::TraversalMantle,ContextStance::Any,false,true,true,36},
    {"ai_mantle_on_40",ContextKind::TraversalMantle,ContextStance::Any,false,true,true,40},
    {"ai_mantle_on_48",ContextKind::TraversalMantle,ContextStance::Any,false,true,true,48},
    {"ai_mantle_on_52",ContextKind::TraversalMantle,ContextStance::Any,false,true,true,52},
    {"ai_mantle_on_56",ContextKind::TraversalMantle,ContextStance::Any,false,true,true,56},
    {"ai_mantle_over_36",ContextKind::TraversalMantle,ContextStance::Any,false,true,true,36,0,true},
    {"ai_jump_down_36",ContextKind::Jump,ContextStance::Any,false,true,true,36},
    {"ai_jump_down_40",ContextKind::Jump,ContextStance::Any,false,true,true,40},
    {"ai_jump_down_56",ContextKind::Jump,ContextStance::Any,false,true,true,56},
    {"ai_jump_down_96",ContextKind::Jump,ContextStance::Any,false,true,true,96},
    {"ai_wounded_run_f_01",ContextKind::WoundedWalk,ContextStance::Stand,true,true},
    {"ai_run_lowready_f_wounded",ContextKind::WoundedWalk,ContextStance::Stand,true,true}
});

inline const ContextDescriptor* findContextDescriptor(std::string_view lowercaseStem){
    // covercrouch_hide_idlea/idleb and ai_wounded_exposed_idle were numerically
    // complete but produced horizontal/sprawling poses in assembled-BO2 checks,
    // unsuitable for the standing-wounded/crouched-cover contexts requested.
    // Keep them absent; native MP idle is the explicit stationary fallback.
    for(const auto& descriptor:kContextDescriptors)if(descriptor.name==lowercaseStem)return &descriptor;
    return nullptr;
}
inline bool contextRigSupported(std::string_view lowercaseGame){return lowercaseGame=="bo2"||lowercaseGame=="bo2_sp"||lowercaseGame=="mw3";}

struct LoadedContextClip { std::size_t animation{};ContextDescriptor descriptor; };
using ContextLibrary=std::vector<LoadedContextClip>;

struct ContextRequest {
    ContextKind kind{ContextKind::StandAim};
    ContextStance stance{ContextStance::Stand};
    bool alive{true},grounded{true},moving{},rifle{true};
    bool reloading{},mantling{},scenarioActive{},firing{};
    bool coverValidated{},traversalValidated{},mantleOver{},jumpDown{},justDied{},temporaryWounded{};
    float coverHeightIw{},traversalHeightIw{},aimPitchDegrees{};
};

inline bool contextCompatible(const ContextDescriptor& d,const ContextRequest& r){
    if(d.kind!=r.kind||(d.rifleOnly&&!r.rifle)||(d.stance!=ContextStance::Any&&d.stance!=r.stance))return false;
    if(d.kind==ContextKind::Death)return !r.alive&&r.justDied&&d.moving==r.moving;
    if(!r.alive||r.reloading||r.scenarioActive)return false;
    if(d.kind==ContextKind::TraversalMantle)return r.traversalValidated&&!r.firing&&d.over==r.mantleOver&&std::isfinite(r.traversalHeightIw)&&std::abs(d.heightIw-r.traversalHeightIw)<=14;
    // These are specifically authored DROPS, not generic forward jump liftoff.
    if(d.kind==ContextKind::Jump)return r.traversalValidated&&r.jumpDown&&!r.firing&&std::isfinite(r.traversalHeightIw)&&std::abs(d.heightIw-r.traversalHeightIw)<=20;
    if(!r.grounded||r.mantling||d.moving!=r.moving)return false;
    if(d.kind==ContextKind::CoverIdle||d.kind==ContextKind::CoverFire)return r.coverValidated&&std::isfinite(r.coverHeightIw)&&std::abs(d.heightIw-r.coverHeightIw)<=16&&(d.kind==ContextKind::CoverFire?r.firing:!r.firing);
    if(d.kind==ContextKind::StandAim||d.kind==ContextKind::StandFire)return std::isfinite(r.aimPitchDegrees)&&(d.kind==ContextKind::StandFire?r.firing:!r.firing);
    if(d.kind==ContextKind::WoundedWalk||d.kind==ContextKind::WoundedIdle)return r.temporaryWounded&&!r.firing;
    return !r.firing; // Throw is animation-only: no gameplay grenade event here.
}

inline std::optional<std::size_t> selectContextClip(const ContextLibrary& library,const ContextRequest& request,std::uint32_t actorId,std::uint64_t opportunitySequence,float chance=1){
    if(!gameplay::bot::spOpportunityChance(chance,actorId,opportunitySequence,0x435458u+static_cast<unsigned>(request.kind)))return std::nullopt;
    float best=std::numeric_limits<float>::infinity();
    std::size_t count=0,first=0;
    std::array<std::size_t,64> choices;
    for(const auto& clip:library){
        const auto& d=clip.descriptor;if(!contextCompatible(d,request))continue;
        float score=0;
        if(d.kind==ContextKind::TraversalMantle||d.kind==ContextKind::Jump)score=std::abs(d.heightIw-request.traversalHeightIw);
        if(d.kind==ContextKind::StandAim||d.kind==ContextKind::StandFire)score=std::abs(d.pitchDegrees-request.aimPitchDegrees);
        if(score<best-.001f){best=score;count=0;first=static_cast<std::size_t>(&clip-library.data());}
        if(std::abs(score-best)<.001f){if(count<choices.size())choices[count]=clip.animation;++count;}
    }
    if(!count)return std::nullopt;
    auto pick=(static_cast<std::uint64_t>(actorId)*7919+opportunitySequence)%count;
    if(count<=choices.size())return choices[static_cast<std::size_t>(pick)];
    for(std::size_t i=first;i<library.size();++i){
        const auto& clip=library[i];
        const auto& d=clip.descriptor;if(!contextCompatible(d,request))continue;
        float score=0;
        if(d.kind==ContextKind::TraversalMantle||d.kind==ContextKind::Jump)score=std::abs(d.heightIw-request.traversalHeightIw);
        if(d.kind==ContextKind::StandAim||d.kind==ContextKind::StandFire)score=std::abs(d.pitchDegrees-request.aimPitchDegrees);
        if(std::abs(score-best)<.001f&&pick--==0)return clip.animation;
    }
    return std::nullopt;
}

} // namespace cadence::sp
