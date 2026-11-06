#pragma once
#include "scene/CastScene.h"

namespace scene::imported {
// Immutable, import-time anatomical indices. No weapon-name offsets, per-frame
// allocations, translation edits, or bone-length changes.
struct SupportArm {
    int shoulder{-1},elbow{-1},wrist{-1},anchor{-1},sourceWrist{-1},sourceAnchor{-1};
    float sourceUnits{1};
    std::vector<std::size_t> descendants;
};
inline bool supportedGripAction(const Animation& a){
    const bool longGun=a.weapon==WeaponClass::Rifle||a.weapon==WeaponClass::Sniper||a.weapon==WeaponClass::Automatic||
        a.weapon==WeaponClass::Shotgun||a.weapon==WeaponClass::LMG||a.weapon==WeaponClass::G11||a.weapon==WeaponClass::M1216;
    return longGun&&!a.contextual&&((a.action==ActionRole::None&&(a.motion==MotionRole::Idle||a.motion==MotionRole::Walk||a.motion==MotionRole::Run))||a.action==ActionRole::Aim||a.action==ActionRole::Fire);
}
inline bool fitSupportArm(const SupportArm& arm,const Skeleton& skeleton,std::vector<Transform>& local,std::vector<Mat4>& globals,Vec3 desired){
    if(arm.shoulder<0||arm.elbow<=arm.shoulder||arm.wrist<=arm.elbow||std::size_t(arm.wrist)>=globals.size()||local.size()!=globals.size())return false;
    const auto position=[&](int b){return transformPoint(globals[b],{});};
    const auto start=position(arm.shoulder),bend=position(arm.elbow),end=position(arm.wrist);
    const auto upper=bend-start,lower=end-bend,reach=desired-start;
    const float a=length(upper),b=length(lower),distance=length(reach);
    if(!std::isfinite(distance)||a<1e-4f||b<1e-4f||distance<1e-4f)return false;
    if(length(desired-end)<1e-5f)return true;
    const auto axis=reach/distance;
    auto pole=upper-axis*dot(upper,axis);
    if(length(pole)<1e-5f){pole=cross(axis,{0,0,1});if(length(pole)<1e-5f)pole=cross(axis,{0,1,0});}
    pole=normalize(pole);
    const float epsilon=std::min(a,b)*1e-5f,d=std::clamp(distance,std::abs(a-b)+epsilon,a+b-epsilon);
    const float along=(a*a-b*b+d*d)/(2*d),height=std::sqrt(std::max(0.f,a*a-along*along));
    const auto newBend=start+axis*along+pole*height,newEnd=start+axis*d;
    const auto orientation=[](const Mat4& m){Vec3 p,s;Quat q;decomposeAffine(m,p,q,s);return normalize(q);};
    const auto arc=[](Vec3 from,Vec3 to){from=normalize(from);to=normalize(to);const float d=std::clamp(dot(from,to),-1.f,1.f);
        if(d<-.9999f){auto axis=cross(from,Vec3{0,0,1});if(length(axis)<1e-5f)axis=cross(from,Vec3{0,1,0});axis=normalize(axis);return Quat{axis.x,axis.y,axis.z,0};}
        const auto axis=cross(from,to);return normalize(Quat{axis.x,axis.y,axis.z,1+d});};
    const auto shoulder=multiply(arc(upper,newBend-start),orientation(globals[arm.shoulder]));
    const auto elbow=multiply(arc(lower,newEnd-newBend),orientation(globals[arm.elbow]));
    const auto wrist=orientation(globals[arm.wrist]);
    for(const auto index:arm.descendants){const auto parent=skeleton.bones[index].parent;const auto pg=parent>=0?globals[parent]:Mat4::identity();
        if(int(index)==arm.shoulder||int(index)==arm.elbow||int(index)==arm.wrist){const auto pq=orientation(pg);const auto wanted=int(index)==arm.shoulder?shoulder:int(index)==arm.elbow?elbow:wrist;
            local[index].rotation=normalize(multiply(Quat{-pq.x,-pq.y,-pq.z,pq.w},wanted));}
        globals[index]=pg*trs(local[index].position,local[index].rotation,local[index].scale);
    }
    return true;
}
}
