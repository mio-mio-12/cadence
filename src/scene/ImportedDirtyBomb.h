#pragma once
#include "scene/CastScene.h"
#include "scene/ImportedBody.h"
#include "assets/ImportedGamePolicy.h"

namespace scene::imported {
// Complete native generic reloads are torso overlays, not idle poses. Keep
// authored ADD and split stages out; full source_absolute clips already exist.
inline bool classifyDirtyBombBodyReload(Animation& clip,std::string_view source,std::string_view mode,const Skeleton& skeleton){
    if(clip.action!=ActionRole::Reload||clip.domain!=AnimationDomain::PlayerBody)return false;
    clip.domain=AnimationDomain::Other;clip.motion=MotionRole::Unknown;
    clip.contextual=true;
    if(mode!="source_absolute"||clip.tracks.empty()||std::any_of(clip.tracks.begin(),clip.tracks.end(),[](const Track& t){
        return t.mode!=TrackMode::Absolute||t.frames.empty()||t.frames.size()!=(t.property==TrackProperty::Rotation?t.rotationValues.size():t.scalarValues.size());
    }))return false;
    auto name=assets::imported::lower(std::string(source));const auto at=name.rfind('@');
    if(at==std::string::npos||at+1==name.size()||!std::all_of(name.begin()+at+1,name.end(),[](char c){return c>='0'&&c<='9';}))return false;
    name.resize(at);bool matched=false;WeaponClass weapon=WeaponClass::Any;
    for(const auto family:{"genericmale","genericheavy","genericfemale"})for(const auto hands:{"2h","1h"}){
        if(name==std::string("animation/")+family+"/"+family+"_"+hands+"_stand_sf.upk:stand_reload"){
            matched=true;weapon=std::string_view(hands)=="2h"?WeaponClass::Rifle:WeaponClass::Pistol;
        }
    }
    const auto mask=nativeBodyTorsoMask(skeleton);
    if(!matched||std::none_of(mask.begin(),mask.end(),[](bool owned){return owned;}))return false;
    clip.domain=AnimationDomain::PlayerTorso;clip.action=ActionRole::Reload;
    clip.weapon=weapon;clip.stance=Stance::Stand;clip.looping=false;clip.ads=false;clip.contextual=false;
    for(auto& track:clip.tracks)track.ownsLayer=track.ownsLayer&&track.boneIndex<mask.size()&&mask[track.boneIndex];
    return true;
}
// Called only for DB body-idle candidates after broad filename discovery.
// Unknown source families remain inspectable, but must not inherit a rifle
// gameplay meaning merely because their exported filename contains "idle".
inline bool classifyDirtyBombBodyIdle(Animation& clip,std::string_view source,std::string_view mode){
    clip.domain=AnimationDomain::Other;clip.motion=MotionRole::Unknown;
    clip.action=ActionRole::None;clip.role=AnimationRole::Unknown;
    clip.weapon=WeaponClass::Any;clip.stance=Stance::Any;clip.contextual=true;
    if(mode!="source_absolute"||clip.tracks.empty()||
       std::any_of(clip.tracks.begin(),clip.tracks.end(),[](const Track& t){return t.mode!=TrackMode::Absolute||t.frames.empty()||
           t.frames.size()!=(t.property==TrackProperty::Rotation?t.rotationValues.size():t.scalarValues.size());}))return false;
    auto name=assets::imported::lower(std::string(source));
    const auto marker=name.rfind('@');
    if(marker==std::string::npos||marker+1==name.size()||
       !std::all_of(name.begin()+marker+1,name.end(),[](char c){return c>='0'&&c<='9';}))return false;
    name.resize(marker);
    for(const auto family:{"genericmale","genericheavy","genericfemale"})for(const auto posture:{"stand","crouch"}){
        const auto prefix=std::string("animation/")+family+"/"+family+"_2h_"+posture+"_sf.upk:"+posture+"_idle_";
        if(!name.starts_with(prefix))continue;
        const auto variant=std::string_view(name).substr(prefix.size());
        if(variant.size()!=2||!std::all_of(variant.begin(),variant.end(),[](char c){return c>='0'&&c<='9';}))continue;
        classifyAnimationName(std::string("pb_rifle_")+posture+"_idle",clip);
        clip.contextual=false;
        return true;
    }
    return false;
}
// Export filenames discard the UPK weapon family. Only admit verified complete
// native 2H recoil; similarly named gadget/1H clips and baked ADD duplicates are
// not interchangeable. Preserve source identity and all authored curves.
inline bool classifyDirtyBombBodyFire(Animation& clip,std::string_view source,std::string_view mode){
    if(mode!="source_absolute"||clip.tracks.empty()||
       std::any_of(clip.tracks.begin(),clip.tracks.end(),[](const Track& t){return t.mode!=TrackMode::Absolute;}))return false;
    auto name=assets::imported::lower(std::string(source));
    const auto marker=name.rfind('@');
    if(marker==std::string::npos||marker+1==name.size()||
       !std::all_of(name.begin()+marker+1,name.end(),[](char c){return c>='0'&&c<='9';}))return false;
    name.resize(marker);
    bool verified=false;
    for(const auto family:{"genericmale","genericheavy"})for(const auto action:{"stand_fire_01","stand_fire_02"})
        verified|=name==std::string("animation/")+family+"/"+family+"_2h_stand_sf.upk:"+action;
    if(!verified)return false;
    classifyAnimationName("pt_rifle_stand_fire",clip);
    clip.domain=AnimationDomain::PlayerTorso;clip.action=ActionRole::Fire;
    clip.motion=MotionRole::Unknown;clip.weapon=WeaponClass::Rifle;
    clip.stance=Stance::Stand;clip.ads=false;clip.looping=false;clip.contextual=false;
    return true;
}
inline bool classifyDirtyBombBodyFire(Animation& clip,std::string_view source,std::string_view mode,const Skeleton& skeleton){
    const auto mask=nativeBodyTorsoMask(skeleton);
    if(std::none_of(mask.begin(),mask.end(),[](bool owned){return owned;})||!classifyDirtyBombBodyFire(clip,source,mode))return false;
    // Full source poses retain their legs when sampled alone, but a firing
    // overlay owns only spine descendants and cannot replace locomotion.
    for(auto& track:clip.tracks)track.ownsLayer=track.ownsLayer&&track.boneIndex<mask.size()&&mask[track.boneIndex];
    return true;
}
inline std::string dbProperty(const cast::Document& doc,const std::string& key){
    for(const auto& root:doc.roots())for(const auto& node:root.children)if(const auto* p=node.findProperty(key);p&&p->stringValue)return *p->stringValue;
    return {};
}
inline bool dbShared(const cast::Document& doc,bool pistol){
    const auto source=assets::imported::lower(dbProperty(doc,"db.source"));
    return source.find(pistol?"viewarms/viewarms_default_1h_anim_":"viewarms/viewarms_default_2h_anim_")!=std::string::npos;
}
inline bool dbHandBone(std::string_view name){return name.starts_with("SkelBone_")||name.starts_with("IKBone_")||name=="Socket_RightHand"||name=="Socket_LeftHand";}
inline std::string dbSplitActionKey(std::string name){
    name=assets::imported::lower(name);
    const auto fire=name.find("_fire_");
    const auto part=name.find("_part_0",fire);
    if(fire==std::string::npos||part==std::string::npos||part+8>name.size())return {};
    auto prefix=name.substr(0,fire);
    const bool zoom=prefix.ends_with("_zoom");
    if(zoom)prefix.resize(prefix.size()-5);
    const auto hands=prefix.find("_sk_viewarms_");
    if(hands!=std::string::npos)prefix.resize(hands);
    // Preserve weapon, hip/ADS, fire variant, and stage identity.
    return prefix+(zoom?"_zoom":"")+name.substr(fire,part+8-fire);
}
inline void mergeDirtyBombSplitHands(CastScene& s){
    const auto hand=[&](const Track& t){return t.boneIndex<s.skeleton.bones.size()&&dbHandBone(s.skeleton.bones[t.boneIndex].name);};
    for(auto& clip:s.animations){
        if(clip.name.ends_with(" [cycle]")||clip.durationFrames==0||clip.framerate<=0||std::any_of(clip.tracks.begin(),clip.tracks.end(),hand))continue;
        const auto key=dbSplitActionKey(clip.sourceName);if(key.empty())continue;
        const auto donor=std::find_if(s.animations.begin(),s.animations.end(),[&](const Animation& a){return &a!=&clip&&a.durationFrames>0&&a.framerate>0&&!a.tracks.empty()&&std::all_of(a.tracks.begin(),a.tracks.end(),hand)&&dbSplitActionKey(a.sourceName)==key;});
        if(donor==s.animations.end())continue;
        // Use the authored hand stage's clock. Keep sufficient frame resolution
        // for both streams rather than dropping dense mechanism keyframes.
        const auto duration=std::max(clip.durationFrames,donor->durationFrames);
        const float gunScale=float(duration)/clip.durationFrames,handScale=float(duration)/donor->durationFrames;
        for(auto& t:clip.tracks)for(auto& f:t.frames)f=static_cast<std::uint32_t>(std::lround(f*gunScale));
        for(auto& n:clip.notifications)for(auto& f:n.frames)f=static_cast<std::uint32_t>(std::lround(f*gunScale));
        for(auto t:donor->tracks){for(auto& f:t.frames)f=static_cast<std::uint32_t>(std::lround(f*handScale));clip.tracks.push_back(std::move(t));}
        for(auto n:donor->notifications){for(auto& f:n.frames)f=static_cast<std::uint32_t>(std::lround(f*handScale));clip.notifications.push_back(std::move(n));}
        clip.durationFrames=duration;clip.framerate=donor->framerate*handScale;
    }
    // ADS exports can contain hands only; their matching hip stage carries
    // the same weapon mechanism. Never borrow a different variant or stage.
    for(auto& clip:s.animations){
        if(clip.durationFrames==0||clip.framerate<=0||clip.tracks.empty()||!std::all_of(clip.tracks.begin(),clip.tracks.end(),hand))continue;
        auto key=dbSplitActionKey(clip.sourceName);const auto zoom=key.find("_zoom_fire_");if(zoom==std::string::npos)continue;
        key.erase(zoom,5);
        const auto donor=std::find_if(s.animations.begin(),s.animations.end(),[&](const Animation& a){return a.durationFrames>0&&a.framerate>0&&!a.name.ends_with(" [cycle]")&&dbSplitActionKey(a.sourceName)==key&&std::any_of(a.tracks.begin(),a.tracks.end(),[&](const Track& t){return !hand(t);});});
        if(donor==s.animations.end())continue;
        const auto duration=std::max(clip.durationFrames,donor->durationFrames);
        const float ownScale=float(duration)/clip.durationFrames,gunScale=float(duration)/donor->durationFrames;
        for(auto& t:clip.tracks)for(auto& f:t.frames)f=static_cast<std::uint32_t>(std::lround(f*ownScale));
        for(auto& n:clip.notifications)for(auto& f:n.frames)f=static_cast<std::uint32_t>(std::lround(f*ownScale));
        for(auto t:donor->tracks)if(!hand(t)){for(auto& f:t.frames)f=static_cast<std::uint32_t>(std::lround(f*gunScale));clip.tracks.push_back(std::move(t));}
        clip.durationFrames=duration;clip.framerate*=ownScale;
    }
}
inline void completeDirtyBombActions(CastScene& s){
    if(s.viewHandsDriverGame!="dirtybomb"&&s.importedViewGame!="dirtybomb")return;
    mergeDirtyBombSplitHands(s);
    for(auto& clip:s.animations){
        const auto name=assets::imported::lower(clip.sourceName);const auto split=name.find("_part_01_");
        if(split==std::string::npos||clip.name.ends_with(" [cycle]"))continue;
        const auto next=name.substr(0,split)+"_part_02_";
        const auto part=std::find_if(s.animations.begin(),s.animations.end(),[&](const auto& a){return assets::imported::lower(a.sourceName).starts_with(next);});
        if(part==s.animations.end()||clip.framerate<=0||part->framerate<=0)continue;
        const float combinedRate=std::max(clip.framerate,part->framerate),ownRate=combinedRate/clip.framerate;
        const auto offset=static_cast<std::uint32_t>(std::lround((clip.durationFrames+1)*ownRate));
        if(ownRate>1){for(auto& t:clip.tracks)for(auto& f:t.frames)f=static_cast<std::uint32_t>(std::lround(f*ownRate));for(auto& n:clip.notifications)for(auto& f:n.frames)f=static_cast<std::uint32_t>(std::lround(f*ownRate));}
        // Each stage clamps its last key independently. Preserve that hold
        // when concatenating: otherwise a constant (one-key) first stage
        // interpolates toward the second stage throughout the firing motion.
        const auto firstEnd=static_cast<std::uint32_t>(std::lround(clip.durationFrames*ownRate));
        for(auto& track:clip.tracks)if(!track.frames.empty()&&track.frames.back()<firstEnd){
            track.frames.push_back(firstEnd);
            if(track.property==TrackProperty::Rotation)track.rotationValues.push_back(track.rotationValues.back());
            else track.scalarValues.push_back(track.scalarValues.back());
        }
        clip.framerate=combinedRate;const auto rate=combinedRate/part->framerate;
        for(const auto& source:part->tracks){auto found=std::find_if(clip.tracks.begin(),clip.tracks.end(),[&](const auto& t){return t.boneIndex==source.boneIndex&&t.property==source.property&&t.mode==source.mode;});
            if(found==clip.tracks.end()){auto copy=source;for(auto& frame:copy.frames)frame=offset+static_cast<std::uint32_t>(std::lround(frame*rate));clip.tracks.push_back(std::move(copy));continue;}
            for(auto frame:source.frames)found->frames.push_back(offset+static_cast<std::uint32_t>(std::lround(frame*rate)));
            found->scalarValues.insert(found->scalarValues.end(),source.scalarValues.begin(),source.scalarValues.end());found->rotationValues.insert(found->rotationValues.end(),source.rotationValues.begin(),source.rotationValues.end());
        }
        for(auto note:part->notifications){for(auto& frame:note.frames)frame=offset+static_cast<std::uint32_t>(std::lround(frame*rate));clip.notifications.push_back(std::move(note));}
        clip.durationFrames=offset+static_cast<std::uint32_t>(std::lround(part->durationFrames*rate));clip.name+=" [cycle]";
    }
    for(auto& clip:s.animations){
        if(clip.name.starts_with("shared: ")||std::any_of(clip.tracks.begin(),clip.tracks.end(),[&](const auto& t){return t.boneIndex<s.skeleton.bones.size()&&s.skeleton.bones[t.boneIndex].name.starts_with("SkelBone_");}))continue;
        const auto action=assets::imported::additionalAction("dirtybomb",clip.sourceName);
        const Animation* donor{};
        for(const auto& a:s.animations)if(a.name=="shared: "+action){donor=&a;break;}
        if(!donor)for(const auto& a:s.animations)if(a.name=="shared: idle"){donor=&a;break;}
        if(!donor)continue;
        for(auto track:donor->tracks){
            // Sample the shared hand action at its own rate.
            for(auto& frame:track.frames)frame=static_cast<std::uint32_t>(std::lround(float(frame)*clip.framerate/donor->framerate));
            clip.tracks.push_back(std::move(track));
        }
        if(donor->name=="shared: "+action&&donor->framerate>0)
            clip.durationFrames=std::max(clip.durationFrames,static_cast<std::uint32_t>(std::lround(donor->durationFrames*clip.framerate/donor->framerate)));
    }
    // Shared ADS poses carry only arms. Keep the current gun's bolt/slide
    // cycle when using those poses, rather than freezing its mechanism.
    const Animation* fire{};
    for(const auto& a:s.animations){
        const auto n=assets::imported::lower(a.sourceName);
        if(a.name.starts_with("shared: ")||assets::imported::additionalAction("dirtybomb",a.sourceName)!="fire"||n.find("_part_02_")!=std::string::npos)continue;
        if(!fire||a.name.ends_with(" [cycle]")){fire=&a;if(a.name.ends_with(" [cycle]"))break;}
    }
    if(!fire)for(const auto& a:s.animations)if(!a.name.starts_with("shared: ")&&assets::imported::additionalAction("dirtybomb",a.sourceName)=="ads_fire"){fire=&a;break;}
    if(fire&&fire->framerate>0)for(auto& a:s.animations)if((a.name=="shared: ads_fire"||a.name=="shared: fire")&&a.framerate>0){
        const auto mechanism=[&](const Track& t){return t.boneIndex<s.skeleton.bones.size()&&!dbHandBone(s.skeleton.bones[t.boneIndex].name);};
        if(std::any_of(a.tracks.begin(),a.tracks.end(),mechanism))continue;
        const float rate=a.framerate/fire->framerate;
        for(auto t:fire->tracks)if(mechanism(t)){for(auto& frame:t.frames)frame=static_cast<std::uint32_t>(std::lround(frame*rate));a.tracks.push_back(std::move(t));}
        // A short slide/bolt track is not the duration of the complete arm
        // action. Truncating the shared recoil recovery here snaps to idle.
        a.durationFrames=std::max(a.durationFrames,static_cast<std::uint32_t>(std::lround(fire->durationFrames*rate)));
        for(auto note:fire->notifications){for(auto& frame:note.frames)frame=static_cast<std::uint32_t>(std::lround(frame*rate));a.notifications.push_back(std::move(note));}
    }
}
}
