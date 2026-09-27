#pragma once
#include "assets/ImportedGamePolicy.h"
#include "scene/CastScene.h"
#include "weapon/WeaponProfile.h"
namespace cadence::imported_actions {
inline void populate(const scene::CastScene& s,weapon::Profile& p,const assets::Asset& asset){
    const auto game=assets::imported::lower(asset.game),prefix=assets::imported::animationPrefix(game,asset.name);
    p.animationPrefix=prefix;
    const auto set=[&](const char* slot,std::initializer_list<const char*> actions){
        if(const auto it=p.animations.find(slot);it!=p.animations.end())for(const auto& a:s.animations)if(a.sourceName==it->second&&!a.tracks.empty())return;
        for(const auto* action:actions){const scene::Animation* best=nullptr;std::size_t bestScore=std::string::npos;
            for(const auto& a:s.animations){const auto name=assets::imported::lower(std::filesystem::path(a.sourceName).stem().string());if(a.tracks.empty())continue;
                const bool normalized=name.starts_with(prefix);
                const auto suffix=normalized?name.substr(prefix.size()):assets::imported::diagnosticAction(game,asset.name,a.sourceName);const std::string expected=action;
                if(suffix!=expected&&!suffix.starts_with(expected+"_"))continue;
                // Normal reload must not silently become reload_empty.
                if(expected=="reload"&&suffix.starts_with("reload_empty"))continue;
                // Opaque exported clip IDs must not select different variants
                // for idle/fire/reload merely because their hashes vary in length.
                const auto score=normalized?suffix.size():10000;
                if(score<bestScore||(score==bestScore&&best&&name<assets::imported::lower(std::filesystem::path(best->sourceName).stem().string()))){best=&a;bestScore=score;}
            }if(best){p.animations[slot]=best->sourceName;return;}
        }
    };
    set("idle",{"idle"});set("fire",{"fire"});set("ads_fire",{"ads_fire","fire"});
    set("pullout",{"pullout","ready"});set("first_raise",{"first_raise","pullout","ready"});set("putaway",{"putaway","put_away"});
    set("reload",{"reload_full","reload"});set("reload_empty",{"reload_empty","reload_full","reload"});
    set("ads_up",{"ads_up"});set("ads_down",{"ads_down"});set("rechamber",{"rechamber"});set("ads_rechamber",{"ads_rechamber","rechamber"});
    set("sprint_in",{"sprint_in","sprint_enter"});set("sprint_loop",{"sprint_loop"});set("sprint_out",{"sprint_out","sprint_exit"});
    set("jump_takeoff",{"jump"});set("jump_land",{"land_soft","land"});set("melee",{"melee"});
    if(assets::imported::sourceFamily(game)&&p.animationVariants["melee"].empty()){
        std::vector<std::string> attacks;
        for(const auto& a:s.animations){if(a.tracks.empty()||a.domain!=scene::AnimationDomain::ViewModel)continue;
            const auto name=assets::imported::lower(std::filesystem::path(a.sourceName).stem().string());
            const auto suffix=name.starts_with(prefix)?name.substr(prefix.size()):assets::imported::diagnosticAction(game,asset.name,a.sourceName);
            if(suffix=="melee"||suffix.starts_with("melee_"))attacks.push_back(a.sourceName);
        }
        std::sort(attacks.begin(),attacks.end());attacks.erase(std::unique(attacks.begin(),attacks.end()),attacks.end());
        if(!attacks.empty())p.animationVariants["melee"]=std::move(attacks);
    }
}
}
