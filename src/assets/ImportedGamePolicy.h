#pragma once
#include "assets/AssetCatalog.h"
#include <algorithm>
#include <cctype>
#include <optional>

namespace assets::imported {
inline std::string utf8(const std::filesystem::path& path){const auto s=path.u8string();return {s.begin(),s.end()};}
inline std::string lower(std::string_view input){std::string s(input);std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return s;}
inline bool sourceFamily(std::string_view game){return game=="cs1.6"||game=="cz"||game=="cscz"||game=="css"||game=="csnz"||game=="cso2";}
inline bool supported(std::string_view game){return sourceFamily(game)||game=="eldewrito";}
inline bool animationPath(const std::filesystem::path& path){
    for(const auto& part:path){const auto n=lower(utf8(part));if(n=="animations"||n=="diagnostic_animations"||n=="anims"||n=="_anims"||n=="animtrees")return true;}
    return false;
}
inline std::string dewBodySemantic(std::string_view filename,std::string_view nativeLabel={}){
    auto n=lower(nativeLabel.empty()?filename:nativeLabel);
    if(n=="combat:airborne_dead"||n=="combat:landing_dead")return "pb_generic_stand_death";
    const auto digits=[](std::string_view s){return !s.empty()&&std::all_of(s.begin(),s.end(),[](unsigned char c){return std::isdigit(c)!=0;});};
    std::string family,action;
    if(n.find(':')!=std::string::npos){
        // CAST n preserves the graph scope. A :2: token is a transition,
        // never an idle/run loop, even when both endpoint names are familiar.
        if(!n.starts_with("combat:"))return {};
        const auto split=n.find(':',7);if(split==std::string::npos)return {};
        family=n.substr(7,split-7);action=n.substr(split+1);
        if(const auto variant=action.find(':');variant!=std::string::npos){
            const auto suffix=action.substr(variant+1);
            if(!suffix.starts_with("var")||!digits(std::string_view(suffix).substr(3)))return {};
            action.resize(variant);
        }
    }else{
        // Fast filename discovery before loading CAST. Require a complete
        // action and numeric export ID; reject graph transitions, including
        // act:guard:idle:2:combat:... whose combat scope is not at the root.
        if(!n.ends_with(".cast"))return {};n.resize(n.size()-5);
        if(n.find("_2_")!=std::string::npos)return {};
        const auto id=n.rfind('_');if(id==std::string::npos||!digits(std::string_view(n).substr(id+1)))return {};n.resize(id);
        const auto scope=n.find("_combat_");if(scope==std::string::npos)return {};
        const auto modelTag=n.rfind('_',scope-1);
        if(scope==0||modelTag==std::string::npos||!digits(std::string_view(n).substr(modelTag+1,scope-modelTag-1)))return {};
        const auto split=n.find('_',scope+8);if(split==std::string::npos)return {};
        family=n.substr(scope+8,split-scope-8);action=n.substr(split+1);
        if(const auto variant=action.rfind("_var");variant!=std::string::npos){
            if(!digits(std::string_view(action).substr(variant+4)))return {};
            action.resize(variant);
        }
    }
    // Native airborne/landing death actions use the "any" family (Elite
    // graphs omit it). They are real body clips, not graph transitions.
    if((family=="any"&&(action=="airborne_dead"||action=="landing_dead"))||((family=="airborne"||family=="landing")&&action=="dead"))return "pb_generic_stand_death";
    if(family!="rifle"&&family!="pistol"&&family!="dual"&&family!="unarmed")return {};
    const auto prefix="pb_"+(family=="dual"?std::string("dualwield"):family=="unarmed"?std::string("generic"):family)+"_stand_";
    if(action=="idle")return prefix+"idle";
    for(const auto motion:{"move","walk"})for(const auto direction:{"front","back","left","right"})if(action==std::string(motion)+"_"+direction)return prefix+(std::string_view(motion)=="move"?"run_":"walk_")+(std::string_view(direction)=="front"?"forward":std::string_view(direction)=="back"?"backward":direction);
    // airborne_arc is a separate non-looping trajectory; it must not steal
    // the ordinary airborne pose. It needs its own explicit slot if supported.
    if(action=="airborne")return prefix+"jump";
    if(action=="land_soft")return prefix+"land";
    return {};
}
inline std::string weaponStem(std::string_view game,std::string_view name){
    auto n=lower(name);
    if(sourceFamily(game)&&n.starts_with("viewmodel_")){
        if(n.starts_with("viewmodel_v_"))return n.substr(10);
        // Exported semantic class is not part of the native weapon identity.
        const auto at=n.find('_',10);return at==std::string::npos?std::string{}:n.substr(at+1);
    }
    if(game=="eldewrito"&&n.ends_with("_weapon"))return n.substr(0,n.size()-7);
    return {};
}
inline std::string animationPrefix(std::string_view game,std::string_view name){
    const auto stem=weaponStem(game,name);if(stem.empty())return {};
    return sourceFamily(game)?lower(name)+"_":stem+"_first_person_"+(stem.ends_with("_dual")?"dual_":"");
}
inline std::string diagnosticAction(std::string_view game,std::string_view weapon,std::string_view clip){
    if(!sourceFamily(game))return {};
    const auto stem=weaponStem(game,weapon),n=lower(clip),prefix="diagnostic_native_"+stem+"_";
    if(stem.empty()||!n.starts_with(prefix))return {};
    auto action=n.substr(prefix.size());const auto id=action.rfind("_clip");
    if(id==std::string::npos)return {};
    action.resize(id);
    // Some CSO2 sequences export explicit magazine variants. Keep discovery
    // narrow: only a terminal _mag followed by digits is a known modifier.
    if(const auto mag=action.rfind("_mag");mag!=std::string::npos){
        const auto number=std::string_view(action).substr(mag+4);
        if(!number.empty()&&std::all_of(number.begin(),number.end(),[](unsigned char c){return std::isdigit(c)!=0;}))action.resize(mag);
    }
    // Native sequence labels can carry an engine weapon label (awm_idle,
    // awpnew_draw). Match complete action tokens, not substring guesses.
    for(const auto token:{"slash","midslash","stab","stap","swipe","swing","melee"}){
        const std::string t=token;std::size_t at{};
        while((at=action.find(t,at))!=std::string::npos){
            const auto tail=action.substr(at+t.size());
            const auto variant=tail.starts_with("_miss")?tail.substr(5):tail.starts_with("_hit")?tail.substr(4):tail;
            if((at==0||action[at-1]=='_')&&(variant.empty()||std::all_of(variant.begin(),variant.end(),[](unsigned char c){return std::isdigit(c)!=0;})))return "melee";
            ++at;
        }
    }
    for(const auto token:{"reload_empty","reload","idle","draw","deploy","shoot","fire","attack","inspect","holster"}){
        std::size_t at=0;
        while((at=action.find(token,at))!=std::string::npos){
            const auto end=at+std::string_view(token).size();
            const auto tail=action.substr(end);
            if((at==0||action[at-1]=='_')&&(tail.empty()||std::all_of(tail.begin(),tail.end(),[](unsigned char c){return std::isdigit(c);}))){
                const std::string_view t=token;
                return t=="draw"||t=="deploy"?"pullout":t=="shoot"||t=="attack"?"fire":t=="holster"?"putaway":std::string(t);
            }
            ++at;
        }
    }
    return {};
}
inline bool matchesAnimation(std::string_view game,std::string_view weapon,std::string_view clip){
    const auto prefix=animationPrefix(game,weapon),n=lower(clip);
    if(prefix.empty()||!n.starts_with(prefix))return false;
    // Exact action boundary prevents ak47_gold from entering the ak47 library.
    const auto action=n.substr(prefix.size());
    for(const auto token:{"idle","fire","reload","pullout","putaway","first_raise","ads","sprint","walk","run","jump","land","fall","melee","inspect","rechamber","ready","put_away","moving","overlays","pitch_and_turn","posing","throw"}){
        const std::string_view t(token);
        if(action==t||action.starts_with(std::string(t)+"_")||action.starts_with(std::string(t)+"."))return true;
    }
    return false;
}
inline std::string worldName(std::string_view game,std::string_view weapon){
    auto stem=weaponStem(game,weapon);
    if(sourceFamily(game)&&stem.starts_with("v_"))return "w_"+stem.substr(2);
    if(game=="eldewrito")if(const auto at=stem.find("_fp_");at!=std::string::npos)return stem.substr(0,at)+"_world";
    return {};
}
inline std::optional<Role> role(std::string_view game,std::string_view name){
    const auto n=lower(name);
    if(!supported(game))return std::nullopt;
    if(sourceFamily(game)){
        if(n.starts_with("hands_")||n.ends_with("_hands")||n.starts_with("hand_")||n.find("_hand_")!=std::string::npos)return Role::ViewHands;
        if(n.starts_with("viewmodel_"))return Role::ViewWeapon;
        if(n.starts_with("w_")||n.starts_with("p_"))return Role::WorldWeapon;
    }else{
        if(n.ends_with("_hands")||n.starts_with("objects_characters_")&&n.find("_fp_")!=std::string::npos)return Role::ViewHands;
        if(n.find("_fp_")!=std::string::npos&&n.ends_with("_weapon"))return Role::ViewWeapon;
        if(n.ends_with("_world"))return Role::WorldWeapon;
    }
    // Bodies require path or embedded skeleton evidence; no JSON dependency.
    return std::nullopt;
}
}
