#pragma once
#include "scene/Math3D.h"
#include <span>
#include <vector>

namespace scene::pose_only {
// A chain's origins follow DONOR rest offsets, never driver joint positions.
// Orientations are already expressed in donor bone frames by the caller.
// An arbitrary contact joint (e.g. wrist) can anchor the chain in world space.
// This primitive does not fit skin, infer units, or solve finger contact IK.
inline bool chain(std::span<const Mat4> donorRest,
                  std::span<const Quat> orientations, std::size_t anchor,
                  Vec3 anchorPosition, float units, std::vector<Mat4>& output) {
    const auto finiteVec=[](Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);};
    if(donorRest.empty()||orientations.size()!=donorRest.size()||anchor>=donorRest.size()||
       !std::isfinite(units)||units<=0||!finiteVec(anchorPosition))return false;
    std::vector<Mat4> result(donorRest.size());
    std::vector<Vec3> offsets(donorRest.size()-1);
    for(std::size_t i=0;i<donorRest.size();++i){
        const auto q=orientations[i];
        const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
        if(!std::isfinite(norm)||norm<1e-12f)return false;
        for(float v:donorRest[i].v)if(!std::isfinite(v))return false;
        // Reject scaled/sheared bind matrices: callers must normalize explicit
        // source units first, not silently reinterpret non-rigid transforms.
        const auto& m=donorRest[i];
        const Vec3 x{m.v[0],m.v[1],m.v[2]},y{m.v[4],m.v[5],m.v[6]},z{m.v[8],m.v[9],m.v[10]};
        if(std::abs(length(x)-1)>1e-3f||std::abs(length(y)-1)>1e-3f||std::abs(length(z)-1)>1e-3f||
           std::abs(dot(x,y))>1e-3f||std::abs(dot(x,z))>1e-3f||std::abs(dot(y,z))>1e-3f||dot(cross(x,y),z)<.999f)return false;
        result[i]=rotation(q);
    }
    for(std::size_t i=0;i<offsets.size();++i){
        const auto local=transformPoint(inverseAffine(donorRest[i]),transformPoint(donorRest[i+1],{}));
        offsets[i]=transformPoint(result[i],local*units);
        if(!finiteVec(offsets[i]))return false;
    }
    const auto place=[&](std::size_t i,Vec3 p){result[i].v[12]=p.x;result[i].v[13]=p.y;result[i].v[14]=p.z;};
    place(anchor,anchorPosition);
    for(std::size_t i=anchor;i>0;--i)place(i-1,transformPoint(result[i],{})-offsets[i-1]);
    for(std::size_t i=anchor+1;i<result.size();++i)place(i,transformPoint(result[i-1],{})+offsets[i-1]);
    output=std::move(result);return true;
}
}
