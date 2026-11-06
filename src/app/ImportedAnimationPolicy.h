#pragma once
#include "assets/ImportedGamePolicy.h"
#include "scene/CastScene.h"
#include "weapon/WeaponProfile.h"
#include "app/ImportedAdsEndpoints.h"
namespace cadence::imported_actions {
inline bool useAdsPose(const scene::CastScene& s,std::size_t index,bool engaged,bool exiting){
    if(index>=s.animations.size())return false;
    const auto& name=s.animations[index].sourceName;
    // Generated endpoints own the whole pose, unlike sparse authored ADS tags.
    // Once released they must not cover reloads or other primary actions.
    const bool generated=name.ends_with("__cadence_ads_up.cast")||name.ends_with("__cadence_ads_down.cast");
    const auto game=assets::imported::lower(s.viewHandsDriverGame.empty()?(s.importedViewGame.empty()?s.animations[index].sourceGame:s.importedViewGame):s.viewHandsDriverGame);
    const auto action=game=="blacksquad"?assets::imported::additionalAction(game,name):std::string{};
    // Black Squad's authored ZoomOut is also a complete pose. Resident class
    // rigs cache its hip endpoint; leaving it active hides Equip underneath.
    const bool completeNativeAds=game=="blacksquad"&&(action=="ads_up"||action=="ads_down"||action=="ads_idle");
    return (!generated&&!completeNativeAds)||engaged||exiting;
}
inline scene::PoseLayer adsFireLayer(const scene::CastScene& s,const weapon::Profile& p,scene::ActionRole role,scene::PoseLayer layer){
    if(role!=scene::ActionRole::Fire||layer.mode!=scene::LayerMode::Override||layer.animation>=s.animations.size())return layer;
    const auto& driver=s.viewHandsDriverGame.empty()?s.importedViewGame:s.viewHandsDriverGame;
    if(driver!="dirtybomb")return layer;
    const auto action=assets::imported::additionalAction(driver,s.animations[layer.animation].sourceName);
    if(action!="fire"&&action!="ads_fire")return layer;
    const auto neutral=p.animations.find(action=="ads_fire"?"ads_idle":"idle");
    if(neutral==p.animations.end())return layer;
    for(std::size_t i=0;i<s.animations.size();++i)if(s.animations[i].sourceName==neutral->second&&!s.animations[i].tracks.empty()){
        // Full-pose shots must contribute recoil, not replace the ADS pose.
        // Frame zero can already contain recoil: use the mapped neutral clip.
        layer.mode=scene::LayerMode::Additive;layer.referenceAnimation=i;layer.referenceFrame=0;break;
    }
    return layer;
}
inline void populate(const scene::CastScene& s,weapon::Profile& p,const assets::Asset& asset){
    const auto game=assets::imported::lower(asset.game),prefix=assets::imported::animationPrefix(game,asset.name);
    p.animationPrefix=prefix;
    const auto set=[&](const char* slot,std::initializer_list<const char*> actions){
        if(const auto it=p.animations.find(slot);it!=p.animations.end())for(const auto& a:s.animations)if(a.sourceName==it->second&&!a.tracks.empty())return;
        for(const auto* action:actions){const scene::Animation* best=nullptr;std::size_t bestScore=std::string::npos;
            for(const auto& a:s.animations){const auto name=assets::imported::lower(std::filesystem::path(a.sourceName).stem().string());if(a.tracks.empty())continue;
                const bool normalized=name.starts_with(prefix);
                const auto suffix=assets::imported::additionalFamily(game)?assets::imported::additionalAction(game,a.sourceName):normalized?name.substr(prefix.size()):assets::imported::diagnosticAction(game,asset.name,a.sourceName);const std::string expected=action;
                if(suffix!=expected&&!suffix.starts_with(expected+"_"))continue;
                // Normal reload must not silently become reload_empty.
                if(expected=="reload"&&suffix.starts_with("reload_empty"))continue;
                // Opaque exported clip IDs must not select different variants
                // for idle/fire/reload merely because their hashes vary in length.
                const auto score=(normalized?suffix.size():10000)+
                    (a.name.starts_with("shared: ")?100000u:0u)+
                    (name.find("_part_02_")!=std::string::npos?1000u:0u);
                if(score<bestScore||(score==bestScore&&best&&name<assets::imported::lower(std::filesystem::path(best->sourceName).stem().string()))){best=&a;bestScore=score;}
            }if(best){p.animations[slot]=best->sourceName;return;}
        }
    };
    set("idle",{"idle"});set("fire",{"fire"});set("ads_fire",{"ads_fire","fire"});
    set("pullout",{"pullout","ready"});set("first_raise",{"first_raise","pullout","ready"});set("putaway",{"putaway","put_away"});
    set("reload",{"reload_full","reload"});set("reload_empty",{"reload_empty","reload_full","reload"});
    set("ads_up",{"ads_up"});set("ads_down",{"ads_down"});set("ads_idle",{"ads_idle"});set("rechamber",{"rechamber"});set("ads_rechamber",{"ads_rechamber","rechamber"});
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
inline void populate(scene::CastScene& s,weapon::Profile& p,const assets::Asset& asset){
    populate(static_cast<const scene::CastScene&>(s),p,asset);
    const auto game=assets::imported::lower(asset.game);
    // Pose-only ADS exports need transitions; don't change scoped snipers,
    // explicit instant scope, or a rig with either authored transition.
    if(game!="dirtybomb"||assets::imported::additionalCategory(game,asset.name)=="sniper"||weapon::isSniper(p.archetype)||p.stats.instantScope||s.dualWield)return;
    const auto resolve=[&](const char* slot){const auto it=p.animations.find(slot);if(it!=p.animations.end())for(std::size_t i=0;i<s.animations.size();++i)if(s.animations[i].sourceName==it->second&&!s.animations[i].tracks.empty())return i;return SIZE_MAX;};
    const auto idle=resolve("idle"),aim=resolve("ads_idle");
    if(idle==SIZE_MAX||aim==SIZE_MAX||resolve("ads_up")!=SIZE_MAX||resolve("ads_down")!=SIZE_MAX)return;
    auto up=adsEndpointClip(s,idle,aim,false,asset.name+"__cadence_ads_up.cast");
    auto down=adsEndpointClip(s,idle,aim,true,asset.name+"__cadence_ads_down.cast");
    p.animations["ads_up"]=up.sourceName;p.animations["ads_down"]=down.sourceName;
    const auto install=[&](scene::Animation clip){
        for(auto& existing:s.animations)if(existing.sourceName==clip.sourceName){existing=std::move(clip);return;}
        s.animations.push_back(std::move(clip));
    };
    install(std::move(up));install(std::move(down));
}
inline void restoreAdsEndpoints(scene::CastScene& s,const weapon::Profile& p){
    if((s.viewHandsDriverGame!="dirtybomb"&&s.importedViewGame!="dirtybomb")||p.stats.instantScope||weapon::isSniper(p.archetype)||s.dualWield)return;
    const auto up=p.animations.find("ads_up"),down=p.animations.find("ads_down");
    if(up==p.animations.end()||down==p.animations.end())return;
    const std::string upSuffix="__cadence_ads_up.cast",downSuffix="__cadence_ads_down.cast";
    if(!up->second.ends_with(upSuffix)||!down->second.ends_with(downSuffix))return;
    const auto model=up->second.substr(0,up->second.size()-upSuffix.size());
    if(down->second!=model+downSuffix||assets::imported::additionalCategory("dirtybomb",model)=="sniper")return;
    const auto resolve=[&](const std::string& name){for(std::size_t i=0;i<s.animations.size();++i)if(s.animations[i].sourceName==name&&!s.animations[i].tracks.empty())return i;return SIZE_MAX;};
    const auto idle=p.animations.find("idle"),aim=p.animations.find("ads_idle");
    if(idle==p.animations.end()||aim==p.animations.end())return;
    const auto hipIndex=resolve(idle->second),aimIndex=resolve(aim->second);
    if(hipIndex==SIZE_MAX||aimIndex==SIZE_MAX)return;
    // Reconstruct only our own saved transition IDs from exact saved endpoints.
    // Never guess another weapon's clips, replace explicit mappings or read a
    // fictitious .cast dependency for these runtime-generated animations.
    for(const auto& pair:{std::pair{up->second,false},std::pair{down->second,true}})
        if(resolve(pair.first)==SIZE_MAX)s.animations.push_back(adsEndpointClip(s,hipIndex,aimIndex,pair.second,pair.first));
}
}
