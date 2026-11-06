#pragma once
#include "scene/CastScene.h"
#include <algorithm>

namespace scene {
// Build only for full donor arms driven by a shoulderless view rig. Preserve
// the donor surface; its own radial envelope sets the camera clearance.
inline ForearmClearanceSupport prepareForearmClearance(const CastScene& s,std::size_t firstBone,std::size_t shoulder,std::size_t elbow,std::size_t wrist){
    ForearmClearanceSupport out;out.shoulder=shoulder;out.elbow=elbow;out.wrist=wrist;
    if(std::max({shoulder,elbow,wrist})>=s.skeleton.bones.size())return out;
    const auto origin=transformPoint(s.skeleton.bones[elbow].restGlobal,{}),end=transformPoint(s.skeleton.bones[wrist].restGlobal,{});
    const auto upperStart=transformPoint(s.skeleton.bones[shoulder].restGlobal,{});
    if(length(end-origin)<1e-5f||length(origin-upperStart)<1e-5f)return out;
    const auto axis=normalize(end-origin),upperAxis=normalize(origin-upperStart);
    for(const auto& mesh:s.meshes)if(!mesh.viewmodelWeapon&&mesh.skinned&&mesh.attachmentIndex<0)for(const auto& v:mesh.vertices){
        float foreWeight=0,upperWeight=0;for(int k=0;k<4;++k){if(v.bones[k]==elbow)foreWeight+=v.weights[k];if(v.bones[k]==shoulder)upperWeight+=v.weights[k];}
        const float weight=foreWeight+upperWeight;if(weight<=.5f)continue;
        const auto p=transformPoint(mesh.modelTransform,v.position),foreDelta=p-origin,upperDelta=p-upperStart;
        // Include the blended elbow/sleeve region. Measure each contribution
        // against its own bind axis: a bent bind pose is not arm thickness.
        const float radial=(foreWeight*length(foreDelta-axis*dot(foreDelta,axis))+upperWeight*length(upperDelta-upperAxis*dot(upperDelta,upperAxis)))/weight;
        out.radius=std::max(out.radius,radial);
    }
    const auto descendant=[&](std::size_t b,std::size_t parent){for(std::size_t n=0;b<s.skeleton.bones.size()&&n<s.skeleton.bones.size();++n){if(b==parent)return true;const auto next=s.skeleton.bones[b].parent;if(next<0)break;b=std::size_t(next);}return false;};
    for(auto b=firstBone;b<s.skeleton.bones.size();++b)if(descendant(b,shoulder)&&!descendant(b,wrist))out.movedBones.push_back(b);
    return out;
}

// Deterministic skin-only correction shared by live and replay rendering.
// No recorded pose, vertices, finger/palm matrices or weapon transforms change.
inline void applyForearmClearance(const CastScene& s,const std::vector<Mat4>& pose,const Mat4& vp,std::vector<Mat4>& skin){
    if(!s.codmRigAdapter||s.codmRigAdapter->forearmClearance.empty())return;
    const Vec3 row0{vp.v[0],vp.v[4],vp.v[8]},row1{vp.v[1],vp.v[5],vp.v[9]},row3{vp.v[3],vp.v[7],vp.v[11]};
    const float determinant=dot(row0,cross(row1,row3));
    if(!std::isfinite(determinant)||std::abs(determinant)<1e-8f)return;
    const auto eye=(cross(row1,row3)*(-vp.v[12])+cross(row3,row0)*(-vp.v[13])+cross(row0,row1)*(-vp.v[15]))/determinant;
    const auto inward=row3+row1;if(length(inward)<1e-5f)return;
    for(const auto& support:s.codmRigAdapter->forearmClearance){
        if(std::max({support.shoulder,support.elbow,support.wrist})>=pose.size()||!std::isfinite(support.radius)||support.radius<=0)continue;
        const auto pivot=transformPoint(pose[support.wrist],{}),proximal=transformPoint(pose[support.shoulder],{}),fore=transformPoint(pose[support.elbow],{})-pivot;
        if(length(fore)<1e-5f||length(proximal-pivot)<1e-5f)continue;
        const auto axis=cross(normalize(fore),normalize(inward)*-1.f);if(length(axis)<1e-5f)continue;
        const auto transform=[&](float angle){return translation(pivot)*rotation(fromAxisAngle(normalize(axis),angle))*translation(pivot*-1.f);};
        const auto clearance=[&](float angle){const auto segment=transformPoint(transform(angle),proximal)-pivot;
            const float t=std::clamp(dot(eye-pivot,segment)/dot(segment,segment),0.f,1.f);return length(pivot+segment*t-eye);};
        // Unit-relative safety margin and gradual onset. A bounded pose bend
        // cannot guarantee clearance for arbitrary user FOV/offset extremes.
        const float margin=support.radius*.16f,onsetWidth=support.radius*.1f,target=support.radius+margin;
        const float distance=clearance(0);if(!std::isfinite(distance)||distance>=target)continue;
        float low=0,high=.35f;if(clearance(high)<=distance)continue;
        if(clearance(high)>=target)for(int n=0;n<20;++n){const float mid=(low+high)*.5f;if(clearance(mid)<target)low=mid;else high=mid;}
        const float onset=std::clamp((target-distance)/onsetWidth,0.f,1.f);
        const auto move=transform(high*onset*onset*(3-2*onset));
        if(!std::all_of(move.v.begin(),move.v.end(),[](float v){return std::isfinite(v);}))continue;
        for(auto b:support.movedBones)if(b<skin.size())skin[b]=move*skin[b];
    }
}
}
