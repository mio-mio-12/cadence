#pragma once
#include "AnimationSet.h"
#include "BotDeathPolicy.h"

namespace cadence {
// Selection-only metadata: no meshes, skeletons, GPU resources or pose curves.
inline scene::Animation selectionMetadata(const scene::Animation& a){
    scene::Animation m;m.name=a.name;m.sourceName=a.sourceName;m.sourceGame=a.sourceGame;
    m.domain=a.domain;m.action=a.action;m.motion=a.motion;m.weapon=a.weapon;m.stance=a.stance;m.direction=a.direction;
    m.ads=a.ads;m.contextual=a.contextual;m.looping=a.looping;m.reloadStyle=a.reloadStyle;
    m.sourceCurveCount=a.sourceCurveCount;m.unmappedCurveCount=a.unmappedCurveCount;m.tracks.resize(a.tracks.size());return m;
}
struct AnimationSetRuntime {
    const scene::CastScene* owner{};
    std::size_t baseCount{};bool initialized{};
    scene::CastScene selection;
    std::map<std::pair<AnimationSet::Key,std::string>,std::size_t> prepared;
    std::map<AnimationSet::Key,std::size_t> bound;
    std::map<AnimationSet::Slot,std::size_t> overrides;
    void begin(const scene::CastScene& actor){if(initialized&&owner==&actor&&baseCount<=actor.animations.size())return;*this={};owner=&actor;initialized=true;baseCount=actor.animations.size();}
    std::optional<std::size_t> find(const scene::AnimationQuery& q)const{
        std::optional<std::size_t> result;int score=-1;
        for(const auto& [slot,index]:overrides){const int s=AnimationSet::match(slot,q);if(s>score){score=s;result=index;}}return result;
    }
    bool assignedPosture(std::size_t index,scene::Stance stance)const{
        for(const auto& [slot,clip]:overrides)if(clip==index&&std::get<0>(slot)!=0&&(!std::get<2>(slot)||std::get<2>(slot)==int(stance)))return true;
        return false;
    }
    void refresh(const scene::CastScene& actor,const AnimationSet& set,bool enabled){
        begin(actor);overrides.clear();selection.animations.clear();selection.animations.reserve(actor.animations.size());
        for(std::size_t i=0;i<actor.animations.size();++i){const auto& a=actor.animations[i];auto m=selectionMetadata(a);
            const AnimationSet::Key key{a.sourceGame,int(a.action),AnimationSet::file(a.sourceName)};
            const auto explicitRule=set.rules.find(key);const auto binding=bound.find(key);
            const bool selectedLegacy=enabled&&explicitRule!=set.rules.end()&&explicitRule->second&&binding!=bound.end()&&binding->second==i;
            if((i>=baseCount&&!selectedLegacy)||(enabled&&!set.enabled(a.sourceGame,a.action,a.sourceName)))m.tracks.clear();
            selection.animations.push_back(std::move(m));
        }
        if(enabled)for(const auto& [slot,key]:set.overrides)if(const auto it=bound.find(key);it!=bound.end()&&it->second<actor.animations.size()&&!actor.animations[it->second].tracks.empty())overrides[slot]=it->second;
    }
    std::vector<BotDeathChoice> deaths(const scene::CastScene& actor,const AnimationSet& set,bool enabled)const{
        auto result=prepareBotDeaths(selection);
        if(enabled)for(const auto& [key,value]:set.rules)if(value&&std::get<1>(key)==int(scene::ActionRole::Death)){
            const auto it=bound.find(key);if(it==bound.end()||it->second>=actor.animations.size())continue;
            const auto& a=actor.animations[it->second];if(a.tracks.empty())continue;
            std::erase_if(result,[&](const auto& c){const auto& old=actor.animations[c.animation];return old.sourceGame==std::get<0>(key)&&AnimationSet::file(old.sourceName)==std::get<2>(key);});
            result.push_back({it->second,std::get<0>(key)+":"+deathKey(std::get<2>(key)),a.motion,a.stance,a.direction,a.weapon,false,false});
        }
        return result;
    }
};
}
