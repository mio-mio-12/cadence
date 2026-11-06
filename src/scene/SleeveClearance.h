#pragma once
#include "scene/CastScene.h"
#include "scene/ForearmClearance.h"
#include <algorithm>

namespace scene {
// Prepared once after donor assembly. Keep every corner of rigid triangles
// touching the sleeve rim: an offscreen vertex is not an offscreen triangle.
inline std::vector<SleeveClearanceSupport> prepareSleeveClearance(const CastScene& s,std::size_t firstBone,const std::array<std::pair<int,int>,2>& arms){
    std::vector<SleeveClearanceSupport> result;
    const auto descendant=[&](std::size_t bone,std::size_t parent){
        for(std::size_t n=0;n<s.skeleton.bones.size()&&bone<s.skeleton.bones.size();++n){
            if(bone==parent)return true;const int next=s.skeleton.bones[bone].parent;if(next<0)break;bone=std::size_t(next);
        }return false;
    };
    for(int side=0;side<2;++side){
        const auto [shoulder,elbow]=arms[side];
        if(shoulder<0||elbow<0||std::size_t(shoulder)>=s.skeleton.bones.size()||std::size_t(elbow)>=s.skeleton.bones.size())continue;
        SleeveClearanceSupport support;support.shoulder=shoulder;support.elbow=elbow;support.sideSign=side?-1.f:1.f;
        const auto start=transformPoint(s.skeleton.bones[support.shoulder].restGlobal,{});
        const auto axis=transformPoint(s.skeleton.bones[support.elbow].restGlobal,{})-start;
        const float armLength=length(axis);if(armLength<1e-5f)continue;
        for(const auto& mesh:s.meshes)if(!mesh.viewmodelWeapon&&mesh.skinned&&mesh.attachmentIndex<0){
            std::vector<bool> rigid(mesh.vertices.size()),proximal(mesh.vertices.size()),included(mesh.vertices.size());
            for(std::size_t i=0;i<mesh.vertices.size();++i){const auto& v=mesh.vertices[i];float weight=0;
                for(int k=0;k<4;++k)if(v.bones[k]==support.shoulder)weight+=v.weights[k];
                rigid[i]=weight>.999f;
                proximal[i]=rigid[i]&&dot(transformPoint(mesh.modelTransform,v.position)-start,axis/armLength)<armLength*.1f;
            }
            for(std::size_t i=0;i+2<mesh.indices.size();i+=3){const auto a=mesh.indices[i],b=mesh.indices[i+1],c=mesh.indices[i+2];
                if(a>=rigid.size()||b>=rigid.size()||c>=rigid.size())continue;
                if(rigid[a]&&rigid[b]&&rigid[c]&&(proximal[a]||proximal[b]||proximal[c]))included[a]=included[b]=included[c]=true;
            }
            for(std::size_t i=0;i<included.size();++i)if(included[i])support.bindPoints.push_back(transformPoint(mesh.modelTransform,mesh.vertices[i].position));
        }
        for(std::size_t i=firstBone;i<s.skeleton.bones.size();++i)if(descendant(i,support.shoulder)&&!descendant(i,support.elbow))support.movedBones.push_back(i);
        if(!support.bindPoints.empty())result.push_back(std::move(support));
    }return result;
}

// Skin-only bounded rigid rotation about the elbow. Never mutate recorded
// poses, vertices, palm/finger transforms, or any bone at/below the elbow.
inline void applySleeveClearance(const CastScene& s,const std::vector<Mat4>& pose,
                                const Mat4& vp,bool flipped,std::vector<Mat4>& skin){
    if(!s.codmRigAdapter)return;
    applyForearmClearance(s,pose,vp,skin);
    for(const auto& support:s.codmRigAdapter->sleeves){
        if(support.shoulder>=pose.size()||support.elbow>=pose.size()||support.shoulder>=skin.size())continue;
        const auto start=transformPoint(pose[support.shoulder],{}),pivot=transformPoint(pose[support.elbow],{});
        const float sign=flipped?-support.sideSign:support.sideSign;
        const Vec3 inward{vp.v[3]+sign*vp.v[0],vp.v[7]+sign*vp.v[4],vp.v[11]+sign*vp.v[8]};
        const float planeLength=length(inward);if(!std::isfinite(planeLength)||planeLength<1e-5f||length(start-pivot)<1e-5f)continue;
        const auto direction=normalize(start-pivot),outward=inward*(-1.f/planeLength);
        const auto tangent=outward-direction*dot(outward,direction);if(length(tangent)<1e-5f)continue;
        const auto axis=normalize(cross(direction,tangent));
        const auto original=skin[support.shoulder];
        const auto transform=[&](float angle){return translation(pivot)*rotation(fromAxisAngle(axis,angle))*translation(pivot*-1.f);};
        const auto clearance=[&](float angle){const auto matrix=transform(angle)*original;float worst=-1e30f;
            for(const auto& p:support.bindPoints)worst=std::max(worst,(dot(inward,transformPoint(matrix,p))+vp.v[15]+sign*vp.v[12])/planeLength);
            return worst;
        };
        if(clearance(0)<=-.25f)continue;
        float low=0,high=.35f;
        if(clearance(high)<=-.25f)for(int n=0;n<18;++n){const float mid=(low+high)*.5f;if(clearance(mid)>-.25f)low=mid;else high=mid;}
        const auto move=transform(high);
        if(!std::all_of(move.v.begin(),move.v.end(),[](float x){return std::isfinite(x);}))continue;
        for(auto bone:support.movedBones)if(bone<skin.size())skin[bone]=move*skin[bone];
    }
}
}
