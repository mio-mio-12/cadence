#include "scene/CastScene.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>

namespace scene {
namespace {
std::optional<std::size_t> actualBone(const Skeleton& skeleton,const std::string& name) {
    // Aliases cannot identify a rig or a deformation helper reliably.
    for(std::size_t i=0;i<skeleton.bones.size();++i)
        if(canonicalName(skeleton.bones[i].name)==canonicalName(name))return i;
    return std::nullopt;
}
Vec3 position(const Mat4& m){return {m.v[12],m.v[13],m.v[14]};}
std::optional<std::size_t> targetBone(const Skeleton& skeleton,std::string name) {
    // IW uses 1-based phalanges; legacy COD uses 0-based phalanges.
    // Detect actual anatomy, never the importer's aliases.
    if(!actualBone(skeleton,"j_index_le_0")&&actualBone(skeleton,"j_index_le_1")){
        for(const auto finger:{"index","mid","ring","pinky","thumb"}){
            const std::string prefix="j_"+std::string(finger)+"_";
            if(name.starts_with(prefix)&&name.size()>2&&name[name.size()-2]=='_'&&name.back()>='0'&&name.back()<='3')++name.back();
        }
    }
    return actualBone(skeleton,name);
}
Mat4 orientation(const Mat4& m){Vec3 p,s;Quat q;decomposeAffine(m,p,q,s);return rotation(q);}
Mat4 frame(Vec3 along,Vec3 across) {
    const auto x=normalize(along);
    auto z=normalize(cross(x,across));
    if(length(z)<.5f)z=normalize(cross(x,std::abs(x.z)<.9f?Vec3{0,0,1}:Vec3{0,1,0}));
    const auto y=normalize(cross(z,x));auto result=Mat4::identity();
    result.v[0]=x.x;result.v[1]=x.y;result.v[2]=x.z;
    result.v[4]=y.x;result.v[5]=y.y;result.v[6]=y.z;
    result.v[8]=z.x;result.v[9]=z.y;result.v[10]=z.z;return result;
}
struct Mapping {std::size_t target,source;Mat4 correction;bool position{};bool fixedCamera{};};
}

float source2RetargetScale(const Skeleton& target,const Skeleton& source) {
    float sourceLength{},targetLength{};
    for(const auto side:{std::string{"le"},std::string{"ri"}}){
        const std::string suffix=side=="le"?"L":"R";
        const auto te=actualBone(target,"j_elbow_"+side),th=actualBone(target,"j_wrist_"+side);
        const auto se=actualBone(source,"arm_lower_"+suffix),sh=actualBone(source,"hand_"+suffix);
        if(!te||!th||!se||!sh)return 1.0f;
        targetLength+=length(target.bones[*te].restLocal.position)+length(target.bones[*th].restLocal.position);
        sourceLength+=length(source.bones[*se].restLocal.position)+length(source.bones[*sh].restLocal.position);
    }
    return sourceLength>1e-4f?targetLength/sourceLength:1.0f;
}

bool retargetSource2AnimationRange(CastScene& target,std::size_t firstAnimation,
                                  const CastScene& source,std::size_t firstSourceAnimation) {
    const auto sw=actualBone(source.skeleton,"wpn"),tw=targetBone(target.skeleton,"tag_weapon");
    if(!sw||!tw||!actualBone(source.skeleton,"hand_L")||!targetBone(target.skeleton,"j_index_le_0")||
       targetBone(target.skeleton,"hand_L")||actualBone(source.skeleton,"j_wrist_le")||
       firstAnimation>=target.animations.size()||firstSourceAnimation>=source.animations.size())return false;
    std::vector<Mapping> mappings;
    for(const auto side:{std::string{"le"},std::string{"ri"}}){
        const std::string suffix=side=="le"?"L":"R";
        const auto ts=targetBone(target.skeleton,"j_shoulder_"+side),te=targetBone(target.skeleton,"j_elbow_"+side),th=targetBone(target.skeleton,"j_wrist_"+side);
        const auto ss=actualBone(source.skeleton,"armUpperShoulder_"+suffix),se=actualBone(source.skeleton,"arm_lower_"+suffix),sh=actualBone(source.skeleton,"hand_"+suffix);
        if(!ts||!te||!th||!ss||!se||!sh)return false;
        const auto ti=targetBone(target.skeleton,"j_index_"+side+"_0"),tp=targetBone(target.skeleton,"j_pinky_"+side+"_0");
        const auto si=actualBone(source.skeleton,"finger_index_0_"+suffix),sp=actualBone(source.skeleton,"finger_pinky_0_"+suffix);
        if(!ti||!tp||!si||!sp)return false;
        const auto targetAcross=position(target.skeleton.bones[*ti].restGlobal)-position(target.skeleton.bones[*tp].restGlobal);
        const auto sourceAcross=position(source.skeleton.bones[*si].restGlobal)-position(source.skeleton.bones[*sp].restGlobal);
        const auto add=[&](std::optional<std::size_t> t,std::optional<std::size_t> s,Vec3 td,Vec3 sd,bool move){
            if(!t||!s||length(td)<1e-5f||length(sd)<1e-5f)return;
            const auto& tr=target.skeleton.bones[*t].restGlobal;const auto& sr=source.skeleton.bones[*s].restGlobal;
            // Match anatomical frames, retaining each rig's bone-axis roll.
            // Animated source global * inverse(source rest) * aligned target rest.
            const auto aligned=frame(sd,sourceAcross)*inverseAffine(frame(td,targetAcross))*orientation(tr);
            mappings.push_back({*t,*s,inverseAffine(orientation(sr))*aligned,move});
        };
        const auto sourceUpper=actualBone(source.skeleton,"arm_upper_"+suffix);
        if(const auto clavicle=targetBone(target.skeleton,"j_clavicle_"+side))
            add(clavicle,ss,position(target.skeleton.bones[*ts].restGlobal)-position(target.skeleton.bones[*clavicle].restGlobal),
                position(source.skeleton.bones[*se].restGlobal)-position(source.skeleton.bones[*ss].restGlobal),true);
        add(ts,sourceUpper,position(target.skeleton.bones[*te].restGlobal)-position(target.skeleton.bones[*ts].restGlobal),
            position(source.skeleton.bones[*se].restGlobal)-position(source.skeleton.bones[*ss].restGlobal),true);
        add(te,se,position(target.skeleton.bones[*th].restGlobal)-position(target.skeleton.bones[*te].restGlobal),
            position(source.skeleton.bones[*sh].restGlobal)-position(source.skeleton.bones[*se].restGlobal),true);
        add(th,sh,(position(target.skeleton.bones[*ti].restGlobal)+position(target.skeleton.bones[*tp].restGlobal))*.5f-position(target.skeleton.bones[*th].restGlobal),
            (position(source.skeleton.bones[*si].restGlobal)+position(source.skeleton.bones[*sp].restGlobal))*.5f-position(source.skeleton.bones[*sh].restGlobal),true);
        const auto tt=targetBone(target.skeleton,"j_wristtwist_"+side),st=actualBone(source.skeleton,"arm_lower_"+suffix+"_TWIST1");
        add(tt,st,position(target.skeleton.bones[*th].restGlobal)-position(target.skeleton.bones[*te].restGlobal),
            position(source.skeleton.bones[*sh].restGlobal)-position(source.skeleton.bones[*se].restGlobal),false);
        for(const auto& names:{std::pair{"index","index"},std::pair{"mid","middle"},std::pair{"ring","ring"},std::pair{"pinky","pinky"},std::pair{"thumb","thumb"}}){
            for(int joint=0;joint<3;++joint){
                const auto t=targetBone(target.skeleton,"j_"+std::string(names.first)+"_"+side+"_"+std::to_string(joint));
                const auto s=actualBone(source.skeleton,"finger_"+std::string(names.second)+"_"+std::to_string(joint)+"_"+suffix);
                const auto tc=targetBone(target.skeleton,"j_"+std::string(names.first)+"_"+side+"_"+std::to_string(joint+1));
                if(!t||!s)continue;
                const auto sc=actualBone(source.skeleton,"finger_"+std::string(names.second)+"_"+std::to_string(joint+1)+"_"+suffix);
                const auto& sr=source.skeleton.bones[*s].restGlobal;
                const auto sd=sc?position(source.skeleton.bones[*sc].restGlobal)-position(sr):transformPoint(orientation(sr),{side=="le"?1.0f:-1.0f,0,0});
                const auto& targetJoint=target.skeleton.bones[*t];
                const auto td=tc?position(target.skeleton.bones[*tc].restGlobal)-position(targetJoint.restGlobal):
                    targetJoint.parent>=0?position(targetJoint.restGlobal)-position(target.skeleton.bones[static_cast<std::size_t>(targetJoint.parent)].restGlobal):Vec3{};
                add(t,s,td,sd,joint==0);
            }
            {
                auto t=targetBone(target.skeleton,"j_"+std::string(names.first)+"palm_"+side);
                if(!t)t=actualBone(target.skeleton,"j_meta"+std::string(names.first)+"_"+side+"_1");
                const auto s=actualBone(source.skeleton,"finger_"+std::string(names.second)+"_meta_"+suffix);
                const auto tc=targetBone(target.skeleton,"j_"+std::string(names.first)+"_"+side+"_0"),sc=actualBone(source.skeleton,"finger_"+std::string(names.second)+"_0_"+suffix);
                if(t&&s&&tc&&sc)add(t,s,position(target.skeleton.bones[*tc].restGlobal)-position(target.skeleton.bones[*t].restGlobal),position(source.skeleton.bones[*sc].restGlobal)-position(source.skeleton.bones[*s].restGlobal),true);
            }
        }
    }
    const float unitScale=source2RetargetScale(target.skeleton,source.skeleton);
    const auto targetCamera=targetBone(target.skeleton,"tag_camera");
    const auto targetOrigin=targetCamera?position(target.skeleton.bones[*targetCamera].restGlobal):Vec3{};
    mappings.push_back({*tw,*sw,Mat4::identity(),true});
    // These Source 2 exports have no camera bone: native playback uses the
    // origin, +X forward, +Z up. Do not inherit COD's bind-camera height/roll.
    if(const auto camera=targetCamera)
        mappings.push_back({*camera,*sw,Mat4::identity(),true,true});
    std::sort(mappings.begin(),mappings.end(),[](const Mapping& a,const Mapping& b){return a.target<b.target;});
    const auto count=std::min(target.animations.size()-firstAnimation,source.animations.size()-firstSourceAnimation);
    for(std::size_t a=0;a<count;++a){
        auto& animation=target.animations[firstAnimation+a];
        CastScene sourceClip;sourceClip.skeleton=source.skeleton;
        sourceClip.animations.push_back(source.animations[firstSourceAnimation+a]);sourceClip.animations[0].looping=false;
        const auto& original=source.animations[firstSourceAnimation+a];
        if(original.durationFrames>36000)return false;
        // Imported mechanisms retain their authored rotations. Their translation
        // keys must use the same units as the explicitly scaled weapon geometry.
        for(auto& track:animation.tracks){
            if(track.property!=TrackProperty::TranslationX&&track.property!=TrackProperty::TranslationY&&track.property!=TrackProperty::TranslationZ)continue;
            if(track.boneIndex>=target.skeleton.bones.size()||track.boneIndex==*tw)continue;
            auto parent=target.skeleton.bones[track.boneIndex].parent;
            while(parent>=0&&static_cast<std::size_t>(parent)!=*tw)parent=target.skeleton.bones[static_cast<std::size_t>(parent)].parent;
            if(parent>=0)for(auto& value:track.scalarValues)value*=unitScale;
        }
        std::vector<std::array<Track,4>> tracks(mappings.size());
        for(std::size_t m=0;m<mappings.size();++m)for(std::size_t c=0;c<4;++c){auto& t=tracks[m][c];t.boneIndex=mappings[m].target;t.mode=TrackMode::Absolute;t.property=c==3?TrackProperty::Rotation:c==0?TrackProperty::TranslationX:c==1?TrackProperty::TranslationY:TrackProperty::TranslationZ;}
        for(std::uint32_t f=0;f<=original.durationFrames;++f){
            const auto sourcePose=sourceClip.samplePose(0,static_cast<float>(f));
            auto locals=target.sampleLocalPose(firstAnimation+a,static_cast<float>(f));
            for(const auto& m:mappings)locals[m.target]=target.skeleton.bones[m.target].restLocal;
            std::vector<Mat4> globals(locals.size());std::size_t nextMapping=0;
            for(std::size_t b=0;b<locals.size();++b){
                const auto& bone=target.skeleton.bones[b];
                const auto parent=bone.parent>=0?globals[static_cast<std::size_t>(bone.parent)]:Mat4::identity();
                globals[b]=parent*trs(locals[b].position,locals[b].rotation,locals[b].scale);
                if(nextMapping>=mappings.size()||mappings[nextMapping].target!=b)continue;
                const auto m=nextMapping++;const auto& map=mappings[m];
                auto desired=map.fixedCamera?Mat4::identity():orientation(sourcePose[map.source])*map.correction;
                const auto p=map.fixedCamera?targetOrigin:map.position?position(sourcePose[map.source])*unitScale+targetOrigin:position(globals[map.target]);
                desired.v[12]=p.x;desired.v[13]=p.y;desired.v[14]=p.z;
                const auto local=bone.parent>=0?inverseAffine(globals[static_cast<std::size_t>(bone.parent)])*desired:desired;
                Vec3 pos,scale;Quat rot;decomposeAffine(local,pos,rot,scale);
                globals[b]=parent*trs(pos,rot,locals[b].scale);
                for(std::size_t c=0;c<4;++c){auto& t=tracks[m][c];t.frames.push_back(f);if(c==3)t.rotationValues.push_back(rot);else t.scalarValues.push_back(c==0?pos.x:c==1?pos.y:pos.z);}
            }
        }
        std::erase_if(animation.tracks,[&](const Track& t){
            if(t.property==TrackProperty::ScaleX||t.property==TrackProperty::ScaleY||t.property==TrackProperty::ScaleZ)return false;
            return std::any_of(mappings.begin(),mappings.end(),[&](const Mapping& m){return m.target==t.boneIndex;});
        });
        for(auto& set:tracks)for(auto& t:set)animation.tracks.push_back(std::move(t));
        animation.durationFrames=original.durationFrames;animation.framerate=original.framerate;animation.looping=original.looping;
        animation.viewmodelCameraReference=Mat4::identity();
        animation.viewmodelCameraReference->v[12]=targetOrigin.x;
        animation.viewmodelCameraReference->v[13]=targetOrigin.y;
        animation.viewmodelCameraReference->v[14]=targetOrigin.z;
    }
    return true;
}
}
