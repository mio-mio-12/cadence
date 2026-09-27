#pragma once
#include "scene/ImportedBody.h"
#include "scene/ImportedBodyFingers.h"
#include "scene/ColdWarWorld.h"
#include <cmath>

namespace scene::imported {
inline BodyLayout bodyOrCodLayout(const Skeleton& s){
    auto b=bodyLayout(s);if(b)return b;
    const auto find=[&](const std::string& name){const auto i=s.boneByCanonicalName.find(name);return i!=s.boneByCanonicalName.end()?static_cast<int>(i->second):-1;};
    b.pelvis=find("pelvis");if(b.pelvis<0)b.pelvis=find("j_mainroot");
    b.spine=find("j_spinelower");b.head=find("j_head");
    for(int side=0;side<2;++side){const std::string p=side?"_ri":"_le";
        b.shoulder[side]=find("j_shoulder"+p);b.elbow[side]=find("j_elbow"+p);b.hand[side]=find("j_wrist"+p);
        b.thigh[side]=find("j_hip"+p);b.knee[side]=find("j_knee"+p);b.foot[side]=find("j_ankle"+p);}
    return b;
}
inline bool completeBodyLayout(const BodyLayout& b){
    if(b.pelvis<0||b.spine<0||b.head<0)return false;
    for(int side=0;side<2;++side)if(b.shoulder[side]<0||b.elbow[side]<0||b.hand[side]<0||b.thigh[side]<0||b.knee[side]<0||b.foot[side]<0)return false;
    return true;
}
inline Quat bodyDirectionArc(Vec3 from,Vec3 to){
    from=normalize(from);to=normalize(to);const float d=std::clamp(dot(from,to),-1.f,1.f);
    if(d<-.9999f){auto axis=cross(from,Vec3{0,0,1});if(length(axis)<.001f)axis=cross(from,Vec3{0,1,0});axis=normalize(axis);return {axis.x,axis.y,axis.z,0};}
    const auto c=cross(from,to);return normalize(Quat{c.x,c.y,c.z,1+d});
}
inline std::optional<Mat4> bodyPalmRestFrame(const Skeleton& rig,int side){
    const auto layout=bodyOrCodLayout(rig);const int wrist=layout.hand[side];if(wrist<0)return {};
    const auto digit=[&](const char* cod,const char* halo,int number){
        if(layout.family==BodyFamily::Halo)return nativeBodyBone(rig,std::string(side?"r_":"l_")+halo+"_low");
        if(sourceBodyFamily(layout.family)){const std::string prefix=layout.family==BodyFamily::Source?"ValveBiped.Bip01_":layout.family==BodyFamily::Cso2?"CSO2_BipM ":"Bip01 ";const std::string separator=layout.family==BodyFamily::Source?"_":" ";const auto stem=prefix+(side?"R":"L")+separator+"Finger"+std::to_string(number);const auto exact=nativeBodyBone(rig,stem);if(exact>=0)return exact;
            // Variant suffixes belong to finger roots only, not knuckles or accessory rigs.
            for(std::size_t i=0;i<rig.bones.size();++i)if(rig.bones[i].parent==wrist&&rig.bones[i].name.starts_with(stem+"_"))return static_cast<int>(i);return -1;}
        const auto prefix="j_"+std::string(cod)+(side?"_ri_":"_le_");auto index=nativeBodyBone(rig,prefix+"0");if(index<0)index=nativeBodyBone(rig,prefix+"1");return index;
    };
    const int index=digit("index","index",1),middle=digit("mid","middle",2),pinky=digit("pinky","pinky",4),thumb=digit("thumb","thumb",0);
    if(index<0)return {};const auto pos=[&](int bone){return transformPoint(rig.bones[bone].restGlobal,{});};const auto origin=pos(wrist);
    const auto forward=pos(middle>=0?middle:index)-origin;
    // Same handedness as ImportedWorldGrip: index/pinky plane on full rigs,
    // thumb/index plane on GoldSrc's two-digit hands. No weapon-specific fit.
    const auto normal=pinky>=0?cross(pos(index)-origin,pos(pinky)-origin):thumb>=0?cross(pos(thumb)-origin,pos(index)-origin):Vec3{};
    if(length(forward)<1e-5f||length(normal)<1e-5f)return {};
    const auto x=normalize(forward),y=normalize(cross(normal,x)),z=normalize(cross(x,y));if(length(y)<.99f||length(z)<.99f)return {};
    auto frame=Mat4::identity();frame.v[0]=x.x;frame.v[1]=x.y;frame.v[2]=x.z;frame.v[4]=y.x;frame.v[5]=y.y;frame.v[6]=y.z;frame.v[8]=z.x;frame.v[9]=z.y;frame.v[10]=z.z;frame.v[12]=origin.x;frame.v[13]=origin.y;frame.v[14]=origin.z;return frame;
}
// Appends one independently sampled body clip. The native source remains
// immutable and no names/aliases are inserted into either live skeleton.
// Torso clips expose ownership only from the target spine upward; they never
// carry source pelvis translation into locomotion underneath the layer.
inline bool appendRetargetedBody(CastScene& target,std::shared_ptr<const CastScene> original,std::size_t sourceAnimation,std::string& error){
    if(!original||sourceAnimation>=original->animations.size()){error="Missing source body animation";return false;}
    const auto t=bodyOrCodLayout(target.skeleton),s=bodyOrCodLayout(original->skeleton);
    if((!bodyLayout(target.skeleton)&&!bodyLayout(original->skeleton))||!completeBodyLayout(t)||!completeBodyLayout(s)){error="Incomplete or unsupported imported body anatomy";return false;}
    const auto& clip=original->animations[sourceAnimation];
    const bool torso=clip.domain==AnimationDomain::PlayerTorso;
    if((clip.domain!=AnimationDomain::PlayerBody&&!torso)||clip.coldWarWorldPose||original->runtimePoseAdapters.contains(sourceAnimation)){error="Expected unadapted body source clip";return false;}
    const auto pos=[](const Skeleton& rig,int bone){return transformPoint(rig.bones[bone].restGlobal,{});};
    const auto leg=[&](const Skeleton& rig,const BodyLayout& layout){return length(pos(rig,layout.thigh[0])-pos(rig,layout.knee[0]))+length(pos(rig,layout.knee[0])-pos(rig,layout.foot[0]));};
    const float sourceLeg=leg(original->skeleton,s),targetLeg=leg(target.skeleton,t);
    if(sourceLeg<.001f||targetLeg<.001f){error="Degenerate body segment lengths";return false;}
    const float ratio=targetLeg/sourceLeg;
    if(!std::isfinite(ratio)||ratio<.001f||ratio>1000.f){error="Invalid body translation scale";return false;}
    // Normalize donor translations once. Rotations and key times are untouched,
    // so its existing interpolation remains authoritative at fractional frames.
    auto source=std::make_shared<CastScene>();source->skeleton=original->skeleton;source->animations.push_back(clip);
    for(auto& bone:source->skeleton.bones){bone.restLocal.position=bone.restLocal.position*ratio;for(int axis=12;axis<=14;++axis)bone.restGlobal.v[axis]*=ratio;bone.inverseBind=inverseAffine(bone.restGlobal);}
    for(auto& track:source->animations[0].tracks)if(track.property==TrackProperty::TranslationX||track.property==TrackProperty::TranslationY||track.property==TrackProperty::TranslationZ)for(auto& value:track.scalarValues)value*=ratio;
    auto adapter=std::make_shared<ColdWarWorldPose>();adapter->source=source;adapter->animation=0;
    const auto count=target.skeleton.bones.size();adapter->sourceBones.assign(count,-1);adapter->rotationOffsets.resize(count);adapter->translationFrames.resize(count);adapter->globalTranslation.assign(count,false);
    const float yaw=std::atan2(t.authoredForward.y,t.authoredForward.x)-std::atan2(s.authoredForward.y,s.authoredForward.x);
    adapter->actionBasis=fromEulerRadians({0,0,yaw});const auto basis=rotation(adapter->actionBasis);
    for(const auto& bone:source->skeleton.bones)adapter->sourceActionReference.push_back(bone.restGlobal);
    for(const auto& bone:target.skeleton.bones){adapter->targetReference.push_back(bone.restLocal);adapter->targetActionReference.push_back(bone.restGlobal);}
    const auto map=[&](int tb,int sb,int tc=-1,int sc=-1){
        if(tb<0||sb<0)return;adapter->sourceBones[tb]=sb;Vec3 p,scale;Quat from,to;decomposeAffine(basis*source->skeleton.bones[sb].restGlobal,p,from,scale);decomposeAffine(target.skeleton.bones[tb].restGlobal,p,to,scale);
        if(tc>=0&&sc>=0){const auto td=pos(target.skeleton,tc)-pos(target.skeleton,tb),sd=transformPoint(basis,pos(source->skeleton,sc)-pos(source->skeleton,sb));if(length(td)>.001f&&length(sd)>.001f)to=multiply(bodyDirectionArc(td,sd),to);}
        adapter->rotationOffsets[tb]=normalize(multiply(Quat{-from.x,-from.y,-from.z,from.w},to));
    };
    map(t.pelvis,s.pelvis);map(t.spine,s.spine);map(t.head,s.head);adapter->globalTranslation[t.pelvis]=!torso;
    for(int side=0;side<2;++side){map(t.shoulder[side],s.shoulder[side],t.elbow[side],s.elbow[side]);map(t.elbow[side],s.elbow[side],t.hand[side],s.hand[side]);map(t.hand[side],s.hand[side]);map(t.thigh[side],s.thigh[side],t.knee[side],s.knee[side]);map(t.knee[side],s.knee[side],t.foot[side],s.foot[side]);map(t.foot[side],s.foot[side]);}
    for(int side=0;side<2;++side){const auto sourcePalm=bodyPalmRestFrame(source->skeleton,side),targetPalm=bodyPalmRestFrame(target.skeleton,side);if(!sourcePalm||!targetPalm)continue;
        const auto alignment=inverseAffine(source->skeleton.bones[s.hand[side]].restGlobal)*(*sourcePalm)*inverseAffine(*targetPalm)*target.skeleton.bones[t.hand[side]].restGlobal;
        Vec3 position,scale;Quat orientation;decomposeAffine(alignment,position,orientation,scale);adapter->rotationOffsets[t.hand[side]]=normalize(orientation);
    }
    const auto sourceFingers=bodyFingerChains(source->skeleton),targetFingers=bodyFingerChains(target.skeleton);
    adapter->fingerBindings.resize(count);
    const auto orientation=[](const Mat4& m){Vec3 p,s;Quat q;decomposeAffine(m,p,q,s);return normalize(q);};
    const auto conjugate=[](Quat q){return Quat{-q.x,-q.y,-q.z,q.w};};
    for(int side=0;side<2;++side){const auto sourcePalm=bodyPalmRestFrame(source->skeleton,side),targetPalm=bodyPalmRestFrame(target.skeleton,side);if(!sourcePalm||!targetPalm)continue;
        for(int digit=0;digit<5;++digit){if(sourceFingers[side][digit][0]<0||targetFingers[side][digit][0]<0)continue;
            int sourceParent=s.hand[side],targetParent=t.hand[side];
            for(int joint=0;joint<3;++joint){const int sb=sourceFingers[side][digit][joint],tb=targetFingers[side][digit][joint];
                auto& f=adapter->fingerBindings[tb];f.sourceParent=sourceParent;f.targetParent=targetParent;
                f.inverseSourceRestRelative=conjugate(orientation(inverseAffine(source->skeleton.bones[sourceParent].restGlobal)*source->skeleton.bones[sb].restGlobal));
                f.targetRestRelative=orientation(inverseAffine(target.skeleton.bones[targetParent].restGlobal)*target.skeleton.bones[tb].restGlobal);
                f.basis=orientation(inverseAffine(target.skeleton.bones[tb].restGlobal)**targetPalm*inverseAffine(*sourcePalm)*source->skeleton.bones[sb].restGlobal);
                adapter->sourceBones[tb]=sb;sourceParent=sb;targetParent=tb;
            }
        }
    }
    adapter->prepareSampling(target.skeleton);
    auto output=clip;output.tracks.clear();output.coldWarWorldPose=adapter;output.unmappedCurveCount=0;
    for(std::size_t b=0;b<count;++b)if(adapter->sourceBones[b]>=0){Track rotationTrack;rotationTrack.boneIndex=b;rotationTrack.property=TrackProperty::Rotation;rotationTrack.frames={0};rotationTrack.rotationValues={target.skeleton.bones[b].restLocal.rotation};
        if(torso){int ancestor=static_cast<int>(b);while(ancestor>=0&&ancestor!=t.spine)ancestor=target.skeleton.bones[ancestor].parent;rotationTrack.ownsLayer=ancestor==t.spine;}
        output.tracks.push_back(std::move(rotationTrack));
        if(adapter->globalTranslation[b])for(const auto property:{TrackProperty::TranslationX,TrackProperty::TranslationY,TrackProperty::TranslationZ}){Track track;track.boneIndex=b;track.property=property;track.frames={0};track.scalarValues={0};output.tracks.push_back(std::move(track));}}
    target.animations.push_back(std::move(output));error.clear();return true;
}
}
