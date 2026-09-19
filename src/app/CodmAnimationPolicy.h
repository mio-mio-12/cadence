#pragma once
#include "scene/CastScene.h"
#include "weapon/WeaponProfile.h"
#include <algorithm>
#include <cctype>
#include <string_view>

namespace cadence::codm_actions {
inline std::string lower(std::string s);
// Legacy ADS-down clips can be sparse hip bases. Native CODM exports contain
// complete absolute poses: retaining the finished down clip masks every later
// primary action (reload, draw, holster and bolt) with frozen idle transforms.
inline bool useAdsBase(bool nativeCodm,bool engaged,bool exiting){return !nativeCodm||engaged||exiting;}
// Native down endpoints are not necessarily the moving hip-idle pose. Blend
// that residual across the whole exit, rather than introducing a second,
// four-times-faster movement during the last quarter of the clip.
inline float adsDownWeight(float progress){const float t=std::clamp(progress,0.f,1.f);return 1.f-t*t*(3.f-2.f*t);}
inline scene::Mat4 blendAffine(const scene::Mat4& from,const scene::Mat4& to,float t){
    // Fitted legacy bind frames may contain shear. A plain TRS reconstruction
    // changes them even at t=0; retain the affine residual during the blend.
    if(t<=0)return from;if(t>=1)return to;
    scene::Vec3 p,q,s,u;scene::Quat r,v;scene::decomposeAffine(from,p,r,s);scene::decomposeAffine(to,q,v,u);
    const auto a=scene::trs(p,r,s),b=scene::trs(q,v,u);auto result=scene::trs(scene::lerp(p,q,t),scene::slerp(r,v,t),scene::lerp(s,u,t));
    for(int i=0;i<16;++i)result.v[i]+=(from.v[i]-a.v[i])*(1-t)+(to.v[i]-b.v[i])*t;
    return result;
}
inline std::optional<scene::Vec3> muzzle(const scene::CastScene& s,const std::vector<scene::Mat4>& pose){
    for(size_t i=0;i<s.skeleton.bones.size()&&i<pose.size();++i)if(lower(s.skeleton.bones[i].name)=="muzzle_point"){
        const auto& m=pose[i];scene::Vec3 p{m.v[12],m.v[13],m.v[14]};if(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z))return p;
    }
    return std::nullopt;
}
inline std::string lower(std::string s){for(auto& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;}
inline bool belongsToWeapon(std::string clip,std::string weapon,const std::vector<std::string>& models){
    clip=lower(clip);weapon=lower(weapon);
    if(!clip.starts_with(weapon+"_"))return false;
    for(auto model:models){model=lower(model);if(model.size()>weapon.size()&&model.starts_with(weapon+"_")&&clip.starts_with(model+"_"))return false;}
    return true;
}
// Slot assignment does not change the source clip's authored pose space.
inline bool hipBolt(const scene::CastScene& s,std::size_t index){
    if(!s.codmNativeCentimetres||index>=s.animations.size())return false;
    const auto name=lower(std::filesystem::path(s.animations[index].sourceName).stem().string());
    const auto prefix=lower(s.codmNativeWeaponStem)+"_";
    if(!name.starts_with(prefix))return false;
    auto suffix=name.substr(prefix.size());
    if(const auto split=suffix.rfind('_');split!=std::string::npos){const auto hash=suffix.substr(split+1);if(hash.size()>=8&&hash.size()<=16&&std::all_of(hash.begin(),hash.end(),[](unsigned char c){return std::isxdigit(c);}))suffix.resize(split);}
    return suffix=="fire_bolt"||suffix=="rechamber";
}
inline std::vector<scene::Mat4> alignedHipBolt(const scene::CastScene& s,const std::vector<scene::Mat4>& base,std::size_t clip,float frame,float weight){
    // Use the rigid barrel socket, not Bone_RightHand: on e.g. M1887 that
    // parent is static while the actual receiver moves below it during ADS.
    auto anchor=s.skeleton.boneByName.find("Muzzle_point");
    if(anchor==s.skeleton.boneByName.end())anchor=s.skeleton.boneByName.find("muzzle_point");
    if(anchor==s.skeleton.boneByName.end()||base.size()!=s.skeleton.bones.size())return base;
    const auto reference=s.samplePose(clip,0),motion=s.samplePose(clip,frame);
    const auto rigid=[](const scene::Mat4&m){scene::Vec3 p,scale;scene::Quat q;scene::decomposeAffine(m,p,q,scale);return scene::trs(p,q,{1,1,1});};
    const auto carrier=rigid(base[anchor->second])*scene::inverseAffine(rigid(reference[anchor->second]));
    auto result=base;
    for(size_t i=0;i<result.size();++i){const auto& name=s.skeleton.bones[i].name;
        if(name=="tag_view")continue;
        if(name=="tag_camera"||name=="codm_camera_motion"){
            result[i]=blendAffine(base[i],base[i]*scene::inverseAffine(reference[i])*motion[i],std::clamp(weight,0.f,1.f));continue;
        }
        result[i]=blendAffine(base[i],carrier*motion[i],std::clamp(weight,0.f,1.f));
    }
    return result;
}
inline const scene::Animation* find(const scene::CastScene& s,const std::string& prefix,std::string_view suffix){
    const auto wanted=prefix+std::string(suffix)+".cast";
    for(const auto& a:s.animations)if(!a.tracks.empty()&&lower(std::filesystem::path(a.sourceName).filename().string())==wanted)return &a;
    // Exporter hash disambiguation is not an action/variant suffix. Only use a
    // unique hash-suffixed match; never guess between conflicting exports.
    const auto hashedPrefix=prefix+std::string(suffix)+"_";const scene::Animation* hashed=nullptr;
    for(const auto& a:s.animations){
        const auto name=lower(std::filesystem::path(a.sourceName).stem().string());
        if(a.tracks.empty()||!name.starts_with(hashedPrefix))continue;
        const auto hash=name.substr(hashedPrefix.size());
        if(hash.size()<8||hash.size()>16||!std::all_of(hash.begin(),hash.end(),[](unsigned char c){return std::isxdigit(c);}))continue;
        if(hashed)return nullptr;hashed=&a;
    }
    if(hashed)return hashed;
    return nullptr;
}
inline void prepare(scene::CastScene& s){
    if(!s.codmNativeCentimetres)return;
    const auto prefix=lower(s.codmNativeWeaponStem)+"_";
    if(find(s,prefix,"ads_down")||find(s,prefix,"un_aiming_on")||find(s,prefix,"runtime_ads_down"))return;
    const auto* up=find(s,prefix,"ads_up");if(!up||!up->durationFrames)return;
    auto down=*up;
    down.sourceName=prefix+"runtime_ads_down.cast";down.name="ADS down (reversed native ADS up)";
    down.looping=false;down.action=scene::ActionRole::Aim;down.notifications.clear();
    for(auto& track:down.tracks){
        if(std::any_of(track.frames.begin(),track.frames.end(),[&](auto f){return f>down.durationFrames;}))return;
        for(auto& frame:track.frames)frame=down.durationFrames-frame;
        std::reverse(track.frames.begin(),track.frames.end());
        std::reverse(track.scalarValues.begin(),track.scalarValues.end());
        std::reverse(track.rotationValues.begin(),track.rotationValues.end());
    }
    s.animations.push_back(std::move(down));
    s.warnings.push_back("CODM ADS-down fallback: reversed native ADS-up including camera channels; no authored ADS-down export. Source files unchanged.");
}
inline void populate(const scene::CastScene& s,weapon::Profile& p,const std::string& stem){
    const auto prefix=lower(stem)+"_";p.animationPrefix=prefix;
    const auto assign=[&](const char* slot,std::initializer_list<const char*> suffixes){
        // Explicit, still-resolvable weaponfile mappings remain authoritative.
        if(auto it=p.animations.find(slot);it!=p.animations.end()&&!it->second.empty()){
            const auto wanted=lower(std::filesystem::path(it->second).filename().string());
            for(const auto& a:s.animations)if(!a.tracks.empty()&&lower(std::filesystem::path(a.sourceName).filename().string())==wanted)return;
            p.animations.erase(it);
        }
        for(auto suffix:suffixes)if(const auto* a=find(s,prefix,suffix)){p.animations[slot]=a->sourceName;return;}
    };
    assign("idle",{"idle","weapon_idle","idle_pose","pose","for_pose"});
    if(p.stats.fullAuto)assign("fire",{"fire","weapon_fire","fire_single","weapon_fire_single","fire0","weapon_fire0"});else assign("fire",{"fire_single","weapon_fire_single","fire","weapon_fire","fire0","weapon_fire0"});
    if(p.stats.fullAuto)assign("ads_fire",{"ads_fire","aiming_fire_single","aiming_fire_s"});
    else assign("ads_fire",{"aiming_fire_single","ads_fire","aiming_fire_s"});
    assign("ads_up",{"ads_up"});assign("ads_down",{"ads_down","un_aiming_on","runtime_ads_down"});assign("ads_idle",{"ads_idle"});
    assign("rechamber",{"rechamber","fire_bolt"});assign("ads_rechamber",{"ads_rechamber","aiming_bolt","rechamber","fire_bolt"});
    assign("reload",{"reload","weapon_change_clip","change_clip_normal","change_clip_all","change_clip_loop","weapon_change_clip_loop"});
    assign("reload_empty",{"reload_empty","weapon_change_clip_e","change_clip_normal_e","change_clip_e_all","reload","weapon_change_clip","change_clip_loop","weapon_change_clip_loop"});
    assign("reload_start",{"reload_start","change_clip_start"});assign("reload_loop",{"reload_loop","change_clip_loop"});assign("reload_end",{"reload_end","change_clip_end"});
    assign("pullout",{"pullout","weapon_equip","equip_w"});assign("first_raise",{"first_raise","pullout","weapon_equip","equip_w"});assign("putaway",{"putaway","weapon_put_down","put_down_w"});
    assign("sprint_in",{"sprint_in","walk_to_sprint"});assign("sprint_loop",{"sprint_loop","weapon_run","sprint"});assign("sprint_out",{"sprint_out","sprint_to_walk"});
    assign("inspect",{"inspect","inspection_s"});assign("mantle",{"mantle","vaulting_climb"});assign("melee",{"melee"});
    assign("crawl_forward",{"prone_crawl_fwd"});assign("crawl_backward",{"prone_crawl_bwd"});
    assign("crawl_left",{"prone_crawl_lt"});assign("crawl_right",{"prone_crawl_rt"});
    if(!p.animations.contains("ads_fire")&&p.animations.contains("fire"))p.animations["ads_fire"]=p.animations["fire"];
}
inline void defaultTimings(const scene::CastScene& s,weapon::Profile& p){
    const auto seconds=[&](const char* slot,float fallback){
        if(auto it=p.animations.find(slot);it!=p.animations.end())for(const auto& a:s.animations)if(lower(std::filesystem::path(a.sourceName).filename().string())==lower(std::filesystem::path(it->second).filename().string())&&a.framerate>0)return a.durationFrames/a.framerate;
        return fallback;
    };
    p.stats.boltAction=p.animations.contains("rechamber");
    p.stats.rechamberTime=seconds("rechamber",p.stats.rechamberTime);
    p.stats.reloadTime=seconds("reload",p.stats.reloadTime);p.stats.reloadEmptyTime=seconds("reload_empty",p.stats.reloadEmptyTime);
    p.stats.adsIn=seconds("ads_up",p.stats.adsIn);p.stats.adsOut=seconds("ads_down",p.stats.adsOut);
    p.stats.raiseTime=seconds("pullout",p.stats.raiseTime);p.stats.firstRaiseTime=seconds("first_raise",p.stats.firstRaiseTime);
    p.stats.dropTime=seconds("putaway",p.stats.dropTime);
    p.stats.sprintLoopTime=0;p.stats.sprintPlaybackScale=1.f;
}
}
