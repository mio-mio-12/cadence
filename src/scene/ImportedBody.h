#pragma once
#include "scene/CastScene.h"
#include "assets/ImportedGamePolicy.h"
#include <array>
#include <optional>
#include <string_view>

namespace scene::imported {
// Anatomical observations, not animation aliases. Different bind frames must
// still be retargeted; exposing a familiar name to the curve binder is unsafe.
enum class BodyFamily { None, Halo, GoldSrc, Source, Cso2, DirtyBomb, BlackSquad, Source2 };
struct BodyLayout {
    BodyFamily family{BodyFamily::None};
    int pelvis{-1}, spine{-1}, head{-1};
    std::array<int,2> shoulder{-1,-1}, elbow{-1,-1}, hand{-1,-1};
    std::array<int,2> thigh{-1,-1}, knee{-1,-1}, foot{-1,-1};
    Vec3 authoredForward{1,0,0};
    explicit operator bool() const { return family!=BodyFamily::None; }
};
inline int nativeBodyBone(const Skeleton& s,std::string_view name){
    const auto found=s.boneByName.find(std::string(name));
    if(found!=s.boneByName.end()&&found->second<s.bones.size())return static_cast<int>(found->second);
    // GITS uses Bip01_* in animation rigs and Bip01 * in mesh rigs.
    std::string alternate(name);std::replace(alternate.begin(),alternate.end(),' ','_');
    const auto other=s.boneByName.find(alternate);return other!=s.boneByName.end()&&other->second<s.bones.size()?static_cast<int>(other->second):-1;
}
inline BodyLayout bodyLayout(const Skeleton& s){
    BodyLayout b;
    const auto has=[&](std::string_view n){return nativeBodyBone(s,n)>=0;};
    const auto facing=[&](BodyLayout& layout){
        if(layout.thigh[0]<0||layout.thigh[1]<0)return;
        const auto left=transformPoint(s.bones[layout.thigh[0]].restGlobal,{})-transformPoint(s.bones[layout.thigh[1]].restGlobal,{});
        auto forward=cross(left,Vec3{0,0,1});if(length(forward)>.001f)layout.authoredForward=normalize(forward);
    };
    std::string prefix,separator;
    if(has("pelvis")&&has("spine_0")&&has("head_0")&&has("leg_upper_L")&&has("leg_upper_R")){
        b.family=BodyFamily::Source2;b.pelvis=nativeBodyBone(s,"pelvis");b.spine=nativeBodyBone(s,"spine_0");b.head=nativeBodyBone(s,"head_0");
        for(int side=0;side<2;++side){const std::string p=side?"_R":"_L";
            b.shoulder[side]=nativeBodyBone(s,"arm_upper"+p);b.elbow[side]=nativeBodyBone(s,"arm_lower"+p);b.hand[side]=nativeBodyBone(s,"hand"+p);
            b.thigh[side]=nativeBodyBone(s,"leg_upper"+p);b.knee[side]=nativeBodyBone(s,"leg_lower"+p);b.foot[side]=nativeBodyBone(s,"ankle"+p);
        }facing(b);return b;
    }
    if(has("SkelBone_Pelvis")||has("b_Transform")){
        const bool db=has("SkelBone_Pelvis");b.family=db?BodyFamily::DirtyBomb:BodyFamily::BlackSquad;prefix=db?"SkelBone_":"b_";
        b.pelvis=nativeBodyBone(s,prefix+(db?"Pelvis":"Transform"));b.spine=nativeBodyBone(s,prefix+"Spine");b.head=nativeBodyBone(s,prefix+"Head");
        for(int side=0;side<2;++side){const auto p=prefix+(side?"R_":"L_");b.shoulder[side]=nativeBodyBone(s,p+"UpperArm");b.elbow[side]=nativeBodyBone(s,p+"Forearm");b.hand[side]=nativeBodyBone(s,p+"Hand");b.thigh[side]=nativeBodyBone(s,p+"Thigh");b.knee[side]=nativeBodyBone(s,p+"Calf");b.foot[side]=nativeBodyBone(s,p+"Foot");}
        facing(b);return b;
    }
    if(has("pelvis")&&has("l_thigh")&&has("r_thigh")&&has("spine")&&has("head")){
        b.family=BodyFamily::Halo;b.pelvis=nativeBodyBone(s,"pelvis");b.spine=nativeBodyBone(s,"spine");b.head=nativeBodyBone(s,"head");
        for(int side=0;side<2;++side){const std::string p=side?"r_":"l_";
            b.shoulder[side]=nativeBodyBone(s,p+"upperarm");b.elbow[side]=nativeBodyBone(s,p+"forearm");b.hand[side]=nativeBodyBone(s,p+"hand");
            b.thigh[side]=nativeBodyBone(s,p+"thigh");b.knee[side]=nativeBodyBone(s,p+"calf");b.foot[side]=nativeBodyBone(s,p+"foot");}
        facing(b);return b;
    }
    if(has("ValveBiped.Bip01_Pelvis")&&has("ValveBiped.Bip01_L_Thigh")&&has("ValveBiped.Bip01_R_Thigh")){b.family=BodyFamily::Source;prefix="ValveBiped.Bip01_";separator="_";}
    else if(has("CSO2_BipM Pelvis")&&has("CSO2_BipM L Thigh")&&has("CSO2_BipM R Thigh")){b.family=BodyFamily::Cso2;prefix="CSO2_BipM ";separator=" ";}
    else if(has("Bip01 Pelvis")&&has("Bip01 L Thigh")&&has("Bip01 R Thigh")){b.family=BodyFamily::GoldSrc;prefix="Bip01 ";separator=" ";}
    else return b;
    b.authoredForward={0,-1,0};b.pelvis=nativeBodyBone(s,prefix+"Pelvis");b.spine=nativeBodyBone(s,prefix+"Spine");
    b.head=nativeBodyBone(s,prefix+(b.family==BodyFamily::GoldSrc?"Head":"Head1"));
    for(int side=0;side<2;++side){const std::string p=prefix+(side?"R":"L")+separator;
        b.shoulder[side]=nativeBodyBone(s,p+"UpperArm");b.elbow[side]=nativeBodyBone(s,p+"Forearm");b.hand[side]=nativeBodyBone(s,p+"Hand");
        b.thigh[side]=nativeBodyBone(s,p+"Thigh");b.knee[side]=nativeBodyBone(s,p+"Calf");b.foot[side]=nativeBodyBone(s,p+"Foot");}
    facing(b);return b;
}
inline bool sourceBodyFamily(BodyFamily family){return family==BodyFamily::Source||family==BodyFamily::GoldSrc||family==BodyFamily::Cso2;}
// A native/retargeted pose remains in its model's authored frame. AI yaw is
// +X-forward; compensate once at presentation, never in animation channels.
// CoD's unclassified canonical bodies already use +X and remain unchanged.
inline float bodyPresentationYaw(const Skeleton& skeleton){
    const auto layout=bodyLayout(skeleton);if(!layout)return 0.f;
    const auto forward=layout.authoredForward;
    if(!std::isfinite(forward.x)||!std::isfinite(forward.y)||forward.x*forward.x+forward.y*forward.y<1e-8f)return 0.f;
    return -std::atan2(forward.y,forward.x);
}
inline bool bipedBodyFamily(BodyFamily family){return sourceBodyFamily(family)||family==BodyFamily::DirtyBomb||family==BodyFamily::BlackSquad;}
inline std::string bodyFingerPrefix(BodyFamily family){return family==BodyFamily::DirtyBomb?"SkelBone_":family==BodyFamily::BlackSquad?"b_":family==BodyFamily::Source?"ValveBiped.Bip01_":family==BodyFamily::Cso2?"CSO2_BipM ":"Bip01 ";}
inline std::string bodyFingerSeparator(BodyFamily family){return family==BodyFamily::Source||family==BodyFamily::DirtyBomb||family==BodyFamily::BlackSquad?"_":" ";}
inline bool sharedBodyBindCompatible(const Skeleton& a,const Skeleton& b){
    if(a.bones.size()!=b.bones.size())return false;
    for(std::size_t i=0;i<a.bones.size();++i){const auto& x=a.bones[i];const auto& y=b.bones[i];
        if(x.name!=y.name||x.parent!=y.parent)return false;
        for(std::size_t k=0;k<16;++k)if(std::abs(x.restGlobal.v[k]-y.restGlobal.v[k])>1e-4f||std::abs(x.inverseBind.v[k]-y.inverseBind.v[k])>1e-4f)return false;
    }return true;
}
// Mesh-container roots differ between otherwise identical character exports.
// They may be merged safely, but never discard an unmatched deform bone (or
// an ancestor of one). Compare shared bones by name, not export ordering.
inline bool sharedBodyAssemblyCompatible(const CastScene& a,const CastScene& b){
    const auto checkDirection=[](const CastScene& from,const CastScene& to){
        const auto& s=from.skeleton;std::vector<bool> required(s.bones.size());
        for(const auto& mesh:from.meshes)if(mesh.skinned)for(const auto& v:mesh.vertices)
            for(std::size_t k=0;k<v.bones.size();++k)if(v.weights[k]>0){
                auto index=static_cast<int>(v.bones[k]);
                for(std::size_t depth=0;depth<s.bones.size()&&index>=0&&static_cast<std::size_t>(index)<s.bones.size();++depth){
                    if(required[index])break;required[index]=true;index=s.bones[index].parent;
                }
            }
        for(std::size_t index=0;index<s.bones.size();++index){const auto& x=s.bones[index];
            const int target=nativeBodyBone(to.skeleton,x.name);
            if(target<0){if(required[index])return false;continue;}
            const auto& y=to.skeleton.bones[target];
            if((x.parent<0)!=(y.parent<0))return false;
            if(x.parent>=0&&(static_cast<std::size_t>(x.parent)>=s.bones.size()||static_cast<std::size_t>(y.parent)>=to.skeleton.bones.size()||s.bones[x.parent].name!=to.skeleton.bones[y.parent].name))return false;
            for(std::size_t k=0;k<16;++k)if(!std::isfinite(x.restGlobal.v[k])||!std::isfinite(y.restGlobal.v[k])||!std::isfinite(x.inverseBind.v[k])||!std::isfinite(y.inverseBind.v[k])||std::abs(x.restGlobal.v[k]-y.restGlobal.v[k])>1e-4f||std::abs(x.inverseBind.v[k]-y.inverseBind.v[k])>1e-4f)return false;
        }return true;
    };
    const auto family=bodyLayout(a.skeleton).family;
    return family!=BodyFamily::None&&family==bodyLayout(b.skeleton).family&&checkDirection(a,b)&&checkDirection(b,a);
}
// Compare only the incoming body's deform ancestry. An assembled target may
// already contain weapons and other variants; those must not force a new bind.
inline bool bodyNeedsIndependentBind(const Skeleton& target,const CastScene& source){
    std::vector<bool> required(source.skeleton.bones.size());
    for(const auto& mesh:source.meshes)if(mesh.skinned)for(const auto& v:mesh.vertices)for(std::size_t k=0;k<v.weights.size();++k)if(v.weights[k]>0){
        auto b=static_cast<int>(v.bones[k]);for(std::size_t depth=0;depth<required.size()&&b>=0&&static_cast<std::size_t>(b)<required.size();++depth){if(required[b])break;required[b]=true;b=source.skeleton.bones[b].parent;}
    }
    for(std::size_t i=0;i<required.size();++i)if(required[i]){
        const auto& b=source.skeleton.bones[i];const auto index=nativeBodyBone(target,b.name);if(index<0)return true;const auto& t=target.bones[index];
        if((b.parent<0)!=(t.parent<0))return true;
        if(b.parent>=0&&source.skeleton.bones[b.parent].name!=target.bones[t.parent].name)return true;
        for(std::size_t k=0;k<16;++k)if(!std::isfinite(b.restGlobal.v[k])||!std::isfinite(t.restGlobal.v[k])||std::abs(b.restGlobal.v[k]-t.restGlobal.v[k])>1e-4f||std::abs(b.inverseBind.v[k]-t.inverseBind.v[k])>1e-4f)return true;
    }return false;
}
// Full-body only. Source-family CAST exports retain inches and -Y facing,
// unlike their +X-normalized viewmodels. Cadence/CoD world bodies use cm/+X.
inline bool normalizeSourceBody(CastScene& s){
    const bool source2=bodyLayout(s.skeleton).family==BodyFamily::Source2;
    if((!sourceBodyFamily(bodyLayout(s.skeleton).family)&&!source2)||s.importedTranslationScale!=1.f)return false;
    constexpr float scale=2.54f;const auto q=fromEulerRadians({0,0,source2?0.f:kPi*.5f});const auto basis=rotation(q),inverseBasis=inverseAffine(basis);
    for(auto& mesh:s.meshes){for(auto& v:mesh.vertices){v.position=transformPoint(basis,v.position*scale);v.normal=transformPoint(basis,v.normal);}for(int k=12;k<=14;++k)mesh.modelTransform.v[k]*=scale;mesh.modelTransform=basis*mesh.modelTransform*inverseBasis;}
    for(auto& bone:s.skeleton.bones){bone.restLocal.position=bone.restLocal.position*scale;bone.absoluteTranslationOffset=bone.absoluteTranslationOffset*scale;
        for(int k=12;k<=14;++k)bone.restGlobal.v[k]*=scale;bone.restGlobal=basis*bone.restGlobal;bone.inverseBind=inverseAffine(bone.restGlobal);
        if(bone.parent<0){bone.restLocal.position=transformPoint(basis,bone.restLocal.position);bone.restLocal.rotation=multiply(q,bone.restLocal.rotation);bone.absoluteTranslationOffset=transformPoint(basis,bone.absoluteTranslationOffset);}}
    s.importedTranslationScale=scale;return true;
}
// Invoke only for newly appended native Source-family body documents, never
// foreign CoD clips. Exported animation roots already face +X (unlike their
// -Y model bind); applying the model basis again turns movement sideways.
inline void normalizeSourceBodyAnimations(CastScene& s,std::size_t first){
    if((!sourceBodyFamily(bodyLayout(s.skeleton).family)&&bodyLayout(s.skeleton).family!=BodyFamily::Source2)||std::abs(s.importedTranslationScale-2.54f)>.0001f)return;
    constexpr float scale=2.54f;
    const auto layout=bodyLayout(s.skeleton);
    const int goldRoot=layout.family==BodyFamily::GoldSrc?nativeBodyBone(s.skeleton,"Bip01"):-1;
    const float rootHeight=goldRoot>=0?s.skeleton.bones[goldRoot].restLocal.position.z:0.f;
    for(std::size_t a=first;a<s.animations.size();++a){if(s.animations[a].coldWarWorldPose)continue;
        // GoldSrc player animations are relative to the entity hull centre.
        // PM_Duck lowers that centre by 18 native units (standing mins -36,
        // duck mins -18). Cadence's actor origin stays at the feet, so restore
        // the posture's origin here, not the standing origin for every clip.
        // ValveSoftware/halflife pm_shared/pm_shared.c, PM_Duck. This changes
        // root placement only: no limb lengths, scales or rotations are fitted.
        const float postureHeight=rootHeight-(goldRoot>=0&&s.animations[a].stance==Stance::Crouch?18.f*scale:0.f);
        for(auto& track:s.animations[a].tracks)if(track.property==TrackProperty::TranslationX||track.property==TrackProperty::TranslationY||track.property==TrackProperty::TranslationZ){
            // GoldSrc animation roots are relative to the model's standing
            // root elevation. Separate exported helper roots share that origin.
            const bool restoreHeight=goldRoot>=0&&track.boneIndex<s.skeleton.bones.size()&&s.skeleton.bones[track.boneIndex].parent<0&&track.property==TrackProperty::TranslationZ&&track.mode==TrackMode::Absolute;
            for(auto& value:track.scalarValues)value=value*scale+(restoreHeight?postureHeight:0.f);
        }
    }
}
inline std::vector<bool> nativeBodyTorsoMask(const Skeleton& s){
    const auto layout=bodyLayout(s);std::vector<bool> result(s.bones.size());
    if(layout.spine<0)return result;
    for(std::size_t b=0;b<s.bones.size();++b){int p=static_cast<int>(b);
        for(std::size_t depth=0;depth<s.bones.size()&&p>=0&&static_cast<std::size_t>(p)<s.bones.size();++depth){
            if(p==layout.spine){result[b]=true;break;}p=s.bones[p].parent;}}
    return result;
}
inline bool classifyDewBodyAnimation(Animation& clip){
    const auto semantic=assets::imported::dewBodySemantic(clip.sourceName,clip.name);
    if(semantic.empty())return false;
    classifyAnimationName(semantic,clip);
    // The native graph exports omit lo on gait cycles. Runtime stops a
    // non-looping clip at its final frame; known locomotion must cycle.
    if(clip.action==ActionRole::None&&(clip.motion==MotionRole::Idle||clip.motion==MotionRole::Walk||clip.motion==MotionRole::Run))clip.looping=true;
    if(clip.action!=ActionRole::None){clip.motion=MotionRole::Unknown;clip.looping=false;}
    return true;
}
inline bool usesNativeHoldingTorso(std::string_view game){return game=="cs1.6"||game=="cz"||game=="cscz"||game=="cso2";}
inline void classifyNativeGaitLayer(const Skeleton& skeleton,Animation& clip){
    if(clip.sourceGame!="cso2"||clip.domain!=AnimationDomain::PlayerBody||clip.action!=ActionRole::None||
       (clip.motion!=MotionRole::Idle&&clip.motion!=MotionRole::Walk&&clip.motion!=MotionRole::Run))return;
    const auto layout=bodyLayout(skeleton);if(layout.family!=BodyFamily::Cso2)return;
    for(const auto& track:clip.tracks)for(int side=0;side<2;++side)
        if(static_cast<int>(track.boneIndex)==layout.thigh[side]||static_cast<int>(track.boneIndex)==layout.knee[side]||static_cast<int>(track.boneIndex)==layout.foot[side])return;
    // Some normalized pb_ clips are weapon-posture layers, not complete gaits.
    // Inspect the native channels before dense retarget support obscures ownership.
    if(!clip.tracks.empty())clip.domain=AnimationDomain::PlayerTorso;
}
inline MotionRole holdingTorsoMotion(std::string_view game,MotionRole base){return game=="cso2"?base:MotionRole::Idle;}
inline bool isNativeHoldingTorso(const Animation& clip){
    if(clip.sourceGame=="cso2")return clip.domain==AnimationDomain::PlayerTorso&&clip.action==ActionRole::None&&(clip.motion==MotionRole::Idle||clip.motion==MotionRole::Walk||clip.motion==MotionRole::Run);
    const auto native=assets::imported::nativeBodyAction(clip.sourceGame,clip.sourceName);
    return native&&native->semantic.ends_with("_idle")&&clip.domain==AnimationDomain::PlayerTorso&&clip.action==ActionRole::None;
}
inline bool classifySourceBodyAnimation(Animation& clip){
    auto name=clip.sourceName;for(auto& c:name)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    // Retain sourceName for replay/editor identity, classify only the label.
    const auto native=assets::imported::nativeBodyAction(clip.sourceGame,name);
    if(native)name=native->semantic;
    if(const auto slash=name.find_last_of("/\\");slash!=std::string::npos)name.erase(0,slash+1);
    if(name.ends_with(".cast"))name.resize(name.size()-5);
    if(!name.starts_with("pb_")&&!name.starts_with("pt_"))return false;
    if(const auto suffix=name.rfind("_clip");suffix!=std::string::npos&&suffix+5<name.size()&&std::all_of(name.begin()+suffix+5,name.end(),[](char c){return c>='0'&&c<='9';}))name.resize(suffix);
    classifyAnimationName(name,clip);
    if(native&&native->semantic.ends_with("_idle")){
        clip.domain=AnimationDomain::PlayerTorso;clip.action=ActionRole::None;
        clip.motion=MotionRole::Idle;clip.looping=true;clip.contextual=false;
        return true;
    }
    const auto has=[&](std::string_view token){return name.find("_"+std::string(token)+"_")!=std::string::npos||name.ends_with("_"+std::string(token));};
    if(has("death")){clip.domain=AnimationDomain::PlayerBody;clip.action=ActionRole::Death;clip.motion=MotionRole::Unknown;clip.looping=false;}
    else for(const auto& action:{std::pair{"reload",ActionRole::Reload},std::pair{"fire",ActionRole::Fire},std::pair{"melee",ActionRole::Melee},std::pair{"flinch",ActionRole::Flinch},std::pair{"aim",ActionRole::Aim}})if(has(action.first)){
        clip.domain=AnimationDomain::PlayerTorso;clip.action=action.second;clip.motion=MotionRole::Unknown;clip.looping=false;break;
    }
    return true;
}
// Socket aliases are resolved on demand only. Never insert these names into
// boneByCanonicalName: that would let unrelated CoD local curves bind directly.
inline std::optional<std::size_t> nativeSocket(const Skeleton& s,std::string_view semantic){
    if(bodyLayout(s).family==BodyFamily::Source2&&(semantic=="tag_weapon_right"||semantic=="tag_weapon_left")){
        const auto i=nativeBodyBone(s,semantic=="tag_weapon_right"?"hand_R":"hand_L");if(i>=0)return static_cast<std::size_t>(i);
    }
    std::string_view name;
    if(semantic=="tag_weapon_right")name="dew2cast_socket__right_hand__0";
    else if(semantic=="tag_weapon_left")name="dew2cast_socket__left_hand__0";
    else if(semantic=="tag_flash")name="dew2cast_socket__muzzle_flash__0";
    else if(semantic=="tag_brass")name="dew2cast_socket__primary_ejection__0";
    else return {};
    const auto index=nativeBodyBone(s,name);if(index<0)return {};
    // A world weapon's left_hand marker is a support-hand target, not its
    // mount. Only a full character may provide a weapon mounting socket.
    if((semantic=="tag_weapon_right"||semantic=="tag_weapon_left")&&bodyLayout(s).family!=BodyFamily::Halo)return {};
    return static_cast<std::size_t>(index);
}
}
