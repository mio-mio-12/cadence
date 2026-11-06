#pragma once
#include "assets/AssetCatalog.h"
#include <algorithm>
#include <cctype>
#include <optional>

namespace assets::imported {
inline std::string utf8(const std::filesystem::path& path){const auto s=path.u8string();return {s.begin(),s.end()};}
inline std::string lower(std::string_view input){std::string s(input);std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return s;}
inline bool sourceFamily(std::string_view game){return game=="cs1.6"||game=="cz"||game=="cscz"||game=="css"||game=="csnz"||game=="cso2";}
inline bool additionalFamily(std::string_view game){return game=="gitsfa"||game=="dirtybomb"||game=="blacksquad";}
inline bool supported(std::string_view game){return sourceFamily(game)||game=="eldewrito"||additionalFamily(game);}
inline std::string unhashed(std::string_view name){
    auto n=lower(name);if(n.ends_with(".cast"))n.resize(n.size()-5);
    if(auto at=n.find("__");at!=std::string::npos)n.resize(at);
    else if(auto at=n.rfind('_');at!=std::string::npos&&n.size()-at==17&&std::all_of(n.begin()+at+1,n.end(),[](unsigned char c){return std::isxdigit(c);}))n.resize(at);
    return n;
}
inline std::string additionalWeaponKey(std::string_view game,std::string_view name){
    auto n=unhashed(name);
    if(game=="gitsfa"&&n.starts_with("weapon_")){n.erase(0,7);auto end=n.find("_1st_");if(end==std::string::npos)end=n.find("_3rd_");if(end!=std::string::npos)return n.substr(0,end);}
    if(game=="dirtybomb"){if(n.starts_with("viewmodel_"))n.erase(0,10);else if(n.starts_with("worldmodel_"))n.erase(0,11);else return {};if(n.ends_with("_world"))n.resize(n.size()-6);return n;}
    if(game=="blacksquad"&&(n.ends_with("_1p")||n.ends_with("_3p")))return n.substr(0,n.size()-3);
    return {};
}
inline std::string additionalCategory(std::string_view game,std::string_view name){
    if(!additionalFamily(game))return {};const auto n=additionalWeaponKey(game,name);if(n.empty())return {};
    if(n.starts_with("cn_")||n.find("knife")!=std::string::npos||n.find("karambit")!=std::string::npos||n.find("balisong")!=std::string::npos||n.find("nimravus")!=std::string::npos)return "knives";
    if(n.starts_with("sr_")||n.find("sniperrifle")!=std::string::npos||n.find("asw338")!=std::string::npos||n.find("steyrhs50")!=std::string::npos)return "sniper";
    if(n.starts_with("pl_")||n.find("pistol")!=std::string::npos)return "pistol";
    if(n.starts_with("sg_")||n.find("shotgun")!=std::string::npos)return "shotgun";
    if(n.starts_with("gr_"))return "grenade";
    if(n.starts_with("smg_")||n.find("smg")!=std::string::npos)return "smg";
    if(n.starts_with("mg_")||n.find("machinegun")!=std::string::npos)return "lmg";
    return "rifle";
}
inline std::string additionalAction(std::string_view game,std::string_view filename){
    auto n=lower(std::filesystem::path(filename).stem().string()),action=std::string{};
    if(game=="gitsfa"){const auto at=n.rfind("_1st_");if(at==std::string::npos)return {};action=n.substr(at+5);if(auto end=action.find("__");end!=std::string::npos)action.resize(end);}
    if(game=="blacksquad"){const auto at=n.find("__mesh_");if(at==std::string::npos)return {};const auto begin=n.find("__",at+7);if(begin==std::string::npos)return {};const auto end=n.find("__",begin+2);action=n.substr(begin+2,end-begin-2);}
    if(game=="dirtybomb"){action=unhashed(n);}
    // Additive deltas require their authored base pose. Never choose one as
    // an absolute idle/fire/reload simply because its filename contains it.
    if(game=="blacksquad"&&action.starts_with("add_"))action.erase(0,4);
    else if(action.starts_with("add_")||action.find("_add_")!=std::string::npos)return {};
    const auto has=[&](std::string_view t){return action==t||action.starts_with(std::string(t)+"_")||action.ends_with("_"+std::string(t))||action.find("_"+std::string(t)+"_")!=std::string::npos;};
    if(has("boltaction")||has("rechamber"))return has("zoom")?"ads_rechamber":"rechamber";
    if(has("reload"))return has("empty")?"reload_empty":"reload";
    if(has("fire"))return has("zoom")?"ads_fire":"fire";
    if(has("swap_end")||has("equip")||has("draw")||has("raise"))return "pullout";
    if(has("swap_start")||has("unequip")||has("holster")||has("lower"))return "putaway";
    if(has("stand_zoom_ready"))return "ads_up";
    if(has("stand_zoom"))return "ads_idle";
    if(has("zoomin"))return "ads_up";
    if(has("zoomout"))return "ads_down";
    if(has("zoomidle"))return "ads_idle";
    if(game=="dirtybomb"&&has("zoom")&&has("idle"))return "ads_idle";
    if(has("idle"))return "idle";
    if(has("weaponinspect")||has("inspect")||(game=="blacksquad"&&has("stare")))return "inspect";
    if(has("jump"))return has("land")||has("down")?"jump_land":"jump_takeoff";
    if(has("sprint_ready"))return "sprint_in";
    if(has("sprint_stop"))return "sprint_out";
    if(has("sprint"))return "sprint_loop";
    if((game=="blacksquad"||game=="gitsfa")&&has("run"))return "sprint_loop";
    if(has("attack")||has("slash")||has("stab"))return "melee";
    return {};
}
struct NativeBodyAction { std::string identity,semantic; };
inline std::optional<NativeBodyAction> nativeBodyAction(std::string_view game,std::string_view filename){
    // CSO2 Idle_Shoot exports contain native DELTA|POST sequences baked
    // against bind, despite absolute CAST tracks. Admitting them here as a
    // complete torso action replaces the holding pose with an A-pose.
    // Require verified native-base composition before enabling that family.
    if(game=="cso2")return {};
    // GoldSrc nine-direction aim exports: the center is blend4. Do not apply
    // this convention to Source/CSO2 or unrelated diagnostic animation files.
    if(game!="cs1.6"&&game!="cz"&&game!="cscz")return {};
    auto n=lower(utf8(std::filesystem::path(filename).filename()));
    if(!n.starts_with("diagnostic_native_")||!n.ends_with(".cast"))return {};
    n.resize(n.size()-5);const auto hash=n.rfind("_clip");
    if(hash==std::string::npos||hash+5==n.size()||!std::all_of(n.begin()+hash+5,n.end(),[](char c){return c>='0'&&c<='9';}))return {};
    n.resize(hash);if(!n.ends_with("_blend4"))return {};n.resize(n.size()-7);
    auto marker=n.find("_crouch_shoot_");bool crouch=marker!=std::string::npos,aim=false;
    std::size_t markerLength=crouch?14:11;
    if(!crouch)marker=n.find("_ref_shoot_");
    if(marker==std::string::npos){aim=true;marker=n.find("_crouch_aim_");crouch=marker!=std::string::npos;
        if(!crouch)marker=n.find("_ref_aim_");markerLength=crouch?12:9;}
    if(marker==std::string::npos||marker<=18)return {};
    const auto weapon=n.substr(marker+markerLength);std::string family;
    for(const auto w:{"ak47","m4a1","aug","sg552","galil","famas","rifle"})if(weapon==w)family="rifle";
    for(const auto w:{"awp","scout","g3sg1","sg550"})if(weapon==w)family="sniper";
    for(const auto w:{"mp5","mp5navy","tmp","p90","mac10","ump45"})if(weapon==w)family="smg";
    for(const auto w:{"glock18","usp","p228","deagle","fiveseven"})if(weapon==w)family="pistol";
    if(weapon=="m249")family="lmg";
    if(weapon=="m3"||weapon=="xm1014")family="shotgun";
    if(weapon=="elite"||weapon=="dualpistols")family="dualwield";
    if(family.empty())return {};
    return NativeBodyAction{n.substr(18,marker-18),"pt_"+family+(crouch?"_crouch_":"_stand_")+(aim?"idle":"fire")};
}
inline std::string additionalBodySemantic(std::string_view game,std::string_view filename){
    if(const auto native=nativeBodyAction(game,filename))return native->semantic;
    auto n=lower(std::filesystem::path(filename).stem().string()),action=std::string{};
    if(game=="gitsfa"){const auto at=n.find("__base_m4a1_3rd_");if(at==std::string::npos)return {};action=n.substr(at+16);if(auto end=action.find("__");end!=std::string::npos)action.resize(end);}
    if(game=="dirtybomb"){if(!n.starts_with("sk_char_"))return {};const auto at=n.find("_base_");if(at==std::string::npos)return {};action=unhashed(n.substr(at+6));}
    if(game=="blacksquad"){if(!n.starts_with("sk_bombeman__")&&!n.starts_with("sk_pm")&&!n.starts_with("sk_vc"))return {};const auto at=n.find("__mesh_");if(at==std::string::npos)return {};const auto begin=n.find("__",at+7);if(begin==std::string::npos)return {};const auto end=n.find("__",begin+2);action=n.substr(begin+2,end-begin-2);}
    if(action.empty()||action.starts_with("add_")||action.find("stunned")!=std::string::npos)return {};
    const auto has=[&](std::string_view t){return action==t||action.starts_with(std::string(t)+"_")||action.ends_with("_"+std::string(t))||action.find("_"+std::string(t)+"_")!=std::string::npos;};
    for(const auto t:{"idle","walk","run","sprint","jump","land","death","die","reload","fire"})if(has(t))return "pb_rifle_"+std::string(has("crouch")?"crouch_":has("prone")?"prone_":"stand_")+(std::string_view(t)=="die"?"death":action);
    return {};
}
inline bool animationPath(const std::filesystem::path& path){
    for(const auto& part:path){const auto n=lower(utf8(part));if(n=="animations"||n=="diagnostic_animations"||n=="anims"||n=="_anims"||n=="animtrees")return true;}
    return false;
}
inline bool dewGroundDeathLabel(std::string label){
    std::replace(label.begin(),label.end(),':','_');
    if(label.starts_with("any_unarmed_"))label.erase(0,12);
    if(!label.starts_with("h_kill_")&&!label.starts_with("s_kill_"))return false;
    label.erase(0,7);
    const auto split=label.find('_');if(split==std::string::npos)return false;
    const auto direction=label.substr(0,split);if(direction!="front"&&direction!="back"&&direction!="left"&&direction!="right")return false;
    label.erase(0,split+1);const auto variant=label.find('_');
    const auto part=label.substr(0,variant);if(part!="head"&&part!="chest"&&part!="gut")return false;
    if(variant==std::string::npos)return true;
    const auto suffix=label.substr(variant+1);
    return suffix.starts_with("var")&&suffix.size()>3&&std::all_of(suffix.begin()+3,suffix.end(),[](unsigned char c){return std::isdigit(c)!=0;});
}
inline std::string dewGroundDeathIdentity(std::string_view filename){
    auto n=lower(filename);if(!n.ends_with(".cast"))return {};n.resize(n.size()-5);
    const auto digits=[](std::string_view s){return !s.empty()&&std::all_of(s.begin(),s.end(),[](unsigned char c){return std::isdigit(c)!=0;});};
    const auto id=n.rfind('_');if(id==std::string::npos||!digits(std::string_view(n).substr(id+1)))return {};n.resize(id);
    for(const auto* marker:{"_any_unarmed_h_kill_","_any_unarmed_s_kill_","_h_kill_","_s_kill_"}){
        const auto at=n.find(marker);if(at==std::string::npos)continue;
        const auto identity=n.substr(0,at);const auto tag=identity.rfind('_');
        if(tag==std::string::npos||tag==0||!digits(std::string_view(identity).substr(tag+1)))continue;
        if(!std::all_of(identity.begin(),identity.end(),[](unsigned char c){return std::isalnum(c)||c=='_';}))continue;
        if(dewGroundDeathLabel(n.substr(at+1)))return identity;
    }
    return {};
}
inline std::string dewBodySemantic(std::string_view filename,std::string_view nativeLabel={}){
    auto n=lower(nativeLabel.empty()?filename:nativeLabel);
    // Verified native directional fatal reactions. Synced kill attacker/victim
    // graphs and contextual airborne/landing clips are not this family.
    if(nativeLabel.empty()?!dewGroundDeathIdentity(n).empty():dewGroundDeathLabel(n))return "pb_generic_stand_death";
    if(n=="combat:airborne_dead"||n=="combat:landing_dead")return "pb_generic_stand_death";
    const auto digits=[](std::string_view s){return !s.empty()&&std::all_of(s.begin(),s.end(),[](unsigned char c){return std::isdigit(c)!=0;});};
    std::string family,action;bool crouched=false;
    if(n.find(':')!=std::string::npos){
        // CAST n preserves the graph scope. A :2: token is a transition,
        // never an idle/run loop, even when both endpoint names are familiar.
        crouched=n.starts_with("crouch:");
        if(!n.starts_with("combat:")&&!crouched)return {};
        const auto split=n.find(':',7);if(split==std::string::npos)return {};
        family=n.substr(7,split-7);action=n.substr(split+1);
        // A native weapon subtype adds one graph level (rifle:br:fire_1).
        // Flatten only that complete action shape, never a :2: transition.
        if(const auto subtype=action.find(':');subtype!=std::string::npos){
            const auto token=action.substr(0,subtype),tail=action.substr(subtype+1);
            if(!token.empty()&&std::all_of(token.begin(),token.end(),[](unsigned char c){return c>='a'&&c<='z';})&&
               (tail.starts_with("fire_")||tail.starts_with("reload_")))action[subtype]='_';
        }
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
        auto scope=n.find("_combat_");
        const auto crouchScope=n.find("_crouch_");
        if(crouchScope!=std::string::npos&&(scope==std::string::npos||crouchScope<scope)){scope=crouchScope;crouched=true;}
        if(scope==std::string::npos)return {};
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
    if(!crouched&&((family=="any"&&(action=="airborne_dead"||action=="landing_dead"))||((family=="airborne"||family=="landing")&&action=="dead")))return "pb_generic_stand_death";
    if(family!="rifle"&&family!="pistol"&&family!="dual"&&family!="unarmed")return {};
    // Crouch is a native graph scope, not a variant of the standing graph.
    // Keep that stance for full-body gaits and weapon torso actions alike.
    const auto prefix="pb_"+(family=="dual"?std::string("dualwield"):family=="unarmed"?std::string("generic"):family)+(crouched?"_crouch_":"_stand_");
    if(action=="idle")return prefix+"idle";
    for(const auto motion:{"move","walk"})for(const auto direction:{"front","back","left","right"})if(action==std::string(motion)+"_"+direction)return prefix+(std::string_view(motion)=="move"?"run_":"walk_")+(std::string_view(direction)=="front"?"forward":std::string_view(direction)=="back"?"backward":direction);
    // airborne_arc is a separate non-looping trajectory; it must not steal
    // the ordinary airborne pose. It needs its own explicit slot if supported.
    if(action=="airborne")return prefix+"jump";
    if(action=="land_soft")return prefix+"land";
    // Ordinary weapon actions are torso layers; preserve the graph family so
    // dual-wield fire cannot become an Any-weapon rifle fallback. Specialized
    // weapon overlays and graph reactions remain unassigned, not guessed idle.
    for(const auto verb:{"fire","reload"}){
        const auto stem=std::string(verb)+"_";
        auto candidate=action;
        if(!candidate.starts_with(stem))if(const auto subtype=candidate.find('_');subtype!=std::string::npos){
            const auto token=candidate.substr(0,subtype);
            if(!token.empty()&&std::all_of(token.begin(),token.end(),[](unsigned char c){return c>='a'&&c<='z';}))candidate.erase(0,subtype+1);
        }
        if(candidate.ends_with("_left"))candidate.resize(candidate.size()-5);
        if(candidate.starts_with(stem)&&digits(std::string_view(candidate).substr(stem.size())))
            return "pt_"+prefix.substr(3)+verb;
    }
    return {};
}
inline std::string dewBodyIdentity(std::string_view filename){
    // Use the same strict graph validation as discovery. A character's
    // combat and crouch graphs share its native bind, not another body's rig.
    const auto n=lower(filename);
    if(const auto identity=dewGroundDeathIdentity(n);!identity.empty())return identity;
    if(dewBodySemantic(n).empty())return {};
    auto marker=n.find("_combat_");
    const auto crouch=n.find("_crouch_");
    if(crouch!=std::string::npos&&(marker==std::string::npos||crouch<marker))marker=crouch;
    return marker==std::string::npos?std::string{}:n.substr(0,marker);
}
inline std::string weaponStem(std::string_view game,std::string_view name){
    if(additionalFamily(game))return additionalWeaponKey(game,name);
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
    if(game=="gitsfa")return "gis_base_1st__";
    if(game=="blacksquad")return stem+"_1p__";
    if(game=="dirtybomb")return "sk_"; // Includes embedded shared ViewArms actions; metadata gates admission.
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
    if(additionalFamily(game)){
        const auto key=additionalWeaponKey(game,weapon),n=lower(clip);if(key.empty()||additionalAction(game,clip).empty())return false;
        if(game=="gitsfa"){const auto split=key.find('_');return split!=std::string::npos&&n.find("__base_"+key.substr(split+1)+"_1st_")!=std::string::npos;}
        if(game=="blacksquad")return n.starts_with(key+"_1p__mesh_");
        return n.starts_with(key+"_");
    }
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
    if(additionalFamily(game)){
        if(n.starts_with("viewhands_")||n.starts_with("hands_")||n.find("viewhand")!=std::string::npos||n.starts_with("sk_vh_"))return Role::ViewHands;
        if(n.find("__att__")!=std::string::npos)return n.find("__3p__")!=std::string::npos?Role::WorldAttachment:Role::ViewAttachment;
        if(n.starts_with("part_"))return n.find("_world_")!=std::string::npos?Role::WorldAttachment:Role::ViewAttachment;
        if(n.starts_with("player_")||n.starts_with("playerpart_")||n.starts_with("sk_pm")||n.starts_with("sk_vc")||n.starts_with("sk_bombeman"))return Role::PlayerModel;
        if(n.starts_with("viewmodel_")||n.find("_1st_")!=std::string::npos||n.find("_1p__")!=std::string::npos)return Role::ViewWeapon;
        if(n.starts_with("worldmodel_")||n.find("_3rd_")!=std::string::npos||n.find("_3p__")!=std::string::npos)return Role::WorldWeapon;
        return std::nullopt;
    }
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
