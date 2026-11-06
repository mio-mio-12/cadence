#pragma once
#include "scene/CastScene.h"
#include "gameplay/BotSpVariety.h"
#include <array>
#include <algorithm>
#include <cctype>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace cadence {
struct BotDeathChoice {
    std::size_t animation{};
    std::string key;
    scene::MotionRole motion{};scene::Stance stance{};scene::Direction direction{};scene::WeaponClass weapon{};
    bool sp{},forwardRun{};
};
struct BotDeathHistory {
    std::array<std::string,4> recent;
    std::uint64_t sequence{};
    std::optional<std::uint64_t> batch;
    std::vector<std::string> batchKeys;
    void beginBatch(std::uint64_t id){if(!batch||*batch!=id){batch=id;batchKeys.clear();}}
    bool excludes(const std::string& key)const{return !key.empty()&&(recent[0]==key||recent[1]==key||std::find(batchKeys.begin(),batchKeys.end(),key)!=batchKeys.end());}
    bool inRotation(const std::string& key)const{
        return !key.empty()&&std::find(recent.begin(),recent.end(),key)!=recent.end();
    }
    void record(std::string key){
        if(batch&&!key.empty())batchKeys.push_back(key);
        for(std::size_t i=recent.size()-1;i>0;--i)recent[i]=std::move(recent[i-1]);
        recent[0]=std::move(key);
    }
};
inline std::string deathKey(std::string name){
    std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    const auto slash=name.find_last_of("/\\");if(slash!=std::string::npos)name.erase(0,slash+1);
    if(name.ends_with(".cast"))name.resize(name.size()-5);
    return name;
}
inline std::vector<BotDeathChoice> prepareBotDeaths(const scene::CastScene& scene){
    std::vector<BotDeathChoice> choices;
    for(std::size_t i=0;i<scene.animations.size();++i){
        const auto& a=scene.animations[i];
        if(a.action!=scene::ActionRole::Death||a.tracks.empty())continue;
        const auto key=deathKey(a.sourceName.empty()?a.name:a.sourceName);
        if(key.find("generic_stand_idle_death")!=std::string::npos||key.find("deathpose")!=std::string::npos||key.find("death_poses")!=std::string::npos||key.find("hostage")!=std::string::npos||key.find("lowviolence")!=std::string::npos)continue;
        if(a.sourceGame=="eldewrito"&&(key.find("airborne_dead")!=std::string::npos||key.find("landing_dead")!=std::string::npos))continue; // Explicit action-set opt-in; not ordinary standing deaths.
        if(key.find("vertigo")!=std::string::npos||key.find("fall_death")!=std::string::npos||key.find("falling")!=std::string::npos||key.find("vehicle")!=std::string::npos)continue;
        const bool sp=key.starts_with("ai_");
        if(key.find("explos")!=std::string::npos||key.find("explode")!=std::string::npos||key.find("blast")!=std::string::npos||key.find("grenade")!=std::string::npos)continue;
        // Conservative gun-death pool: generic T6 launch/flip and melee-impact
        // families are not ordinary bullet reactions even without an explosion tag.
        if(key.find("armslegsforward")!=std::string::npos||key.find("death_flip")!=std::string::npos||key.find("shieldbash")!=std::string::npos||key.find("death_melee")!=std::string::npos)continue;
        if(!sp&&a.domain!=scene::AnimationDomain::PlayerBody&&a.domain!=scene::AnimationDomain::PlayerTorso)continue;
        // Moving SP scenes need root-motion collision support before they can
        // enter this pool. Keep existing validated, in-place SP deaths only.
        if(sp&&key!="ai_death_collapse_in_place"&&key!="ai_death_flatonback")continue;
        if(std::any_of(choices.begin(),choices.end(),[&](const auto& c){return c.key==key;}))continue;
        const bool forwardRun=key=="pb_death_run_forward_crumple"||((a.motion==scene::MotionRole::Sprint||a.motion==scene::MotionRole::Run)&&a.direction==scene::Direction::Forward);
        choices.push_back({i,key,a.motion,a.stance,a.direction,a.weapon,sp,forwardRun});
    }
    return choices;
}
struct BotDeathRequest {
    scene::MotionRole motion{};scene::Stance stance{};scene::Direction direction{};scene::WeaponClass weapon{};
    bool spMode{};float mix{},tolerance{18};std::uint32_t actor{};
};
inline std::optional<std::size_t> selectBotDeath(const std::vector<BotDeathChoice>& choices,BotDeathHistory& history,const BotDeathRequest& r,const std::vector<std::string>& overrides={}){
    const auto sequence=++history.sequence;
    const bool random=r.spMode&&gameplay::bot::spOpportunityChance(r.mix,r.actor,sequence,0x44454144);
    const bool boost=gameplay::bot::spOpportunityChance(.10f,r.actor,sequence,0x52554e);
    const auto eligible=[&](const auto& c){return !history.excludes(c.key)&&(!c.sp||r.spMode);};
    const auto score=[&](const auto& c){
        const int direction=r.direction==scene::Direction::Any?0:
            c.direction==r.direction?36:c.direction==scene::Direction::Any?14:-10;
        return (c.motion==r.motion?50:c.motion==scene::MotionRole::Unknown?8:-8)+
            (c.stance==r.stance?32:c.stance==scene::Stance::Any?10:-12)+direction+
            (c.weapon==r.weapon?22:c.weapon==scene::WeaponClass::Any?8:-5);
    };
    const auto fitsRotation=[&](const auto& c){
        const bool samePosture=c.stance==r.stance||c.stance==scene::Stance::Any||r.stance==scene::Stance::Any;
        const bool stationaryMatch=r.motion!=scene::MotionRole::Idle||c.motion==scene::MotionRole::Idle||c.motion==scene::MotionRole::Unknown;
        return samePosture&&stationaryMatch&&!history.inRotation(c.key);
    };
    constexpr auto noScore=std::numeric_limits<int>::min();
    bool hasBoost=false,hasOverride=false;
    int best=noScore,bestRotation=noScore;
    for(const auto& c:choices)if(eligible(c)){
        hasBoost|=c.forwardRun;
        hasOverride|=std::find(overrides.begin(),overrides.end(),c.key)!=overrides.end();
        if(!c.sp){
            const int match=score(c);
            best=std::max(best,match);
            if(fitsRotation(c))bestRotation=std::max(bestRotation,match);
        }
    }
    // Use the broader rotation when there is a compatible alternative.
    // Strict tolerance, overrides, and the SP random mix retain their policy.
    const bool rotate=r.tolerance>0&&bestRotation!=noScore;
    const auto accepted=[&](const auto& c){
        if(!eligible(c))return false;
        if(boost&&hasBoost)return c.forwardRun;
        if(random)return true;
        if(hasOverride)return std::find(overrides.begin(),overrides.end(),c.key)!=overrides.end();
        if(c.sp)return false;
        return rotate?fitsRotation(c)&&score(c)>=bestRotation-static_cast<int>(r.tolerance):
            score(c)>=best-static_cast<int>(r.tolerance);
    };
    std::size_t count=0;for(const auto& c:choices)if(accepted(c))++count;
    // Never violate the two-death exclusion, even for a deficient library.
    if(!count){history.record({});return {};}
    auto seed=sequence*0x9e3779b97f4a7c15ull+r.actor;seed^=seed>>30;seed*=0xbf58476d1ce4e5b9ull;seed^=seed>>27;
    auto pick=static_cast<std::size_t>(seed%count);
    for(const auto& c:choices)if(accepted(c)&&pick--==0){history.record(c.key);return c.animation;}
    return {};
}
}
