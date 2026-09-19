#pragma once
#include "scene/CastScene.h"

namespace render::visibility {
inline bool finite(scene::Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
inline void include(scene::Bounds& b,scene::Vec3 p){
    if(!b.valid){b={p,p,true};return;}
    b.minimum={std::min(b.minimum.x,p.x),std::min(b.minimum.y,p.y),std::min(b.minimum.z,p.z)};
    b.maximum={std::max(b.maximum.x,p.x),std::max(b.maximum.y,p.y),std::max(b.maximum.z,p.z)};
}
inline bool transform(scene::Bounds& result,const scene::Bounds& source,const scene::Mat4& m){
    if(!source.valid||!std::all_of(m.v.begin(),m.v.end(),[](float f){return std::isfinite(f);})||
        std::abs(m.v[3])+std::abs(m.v[7])+std::abs(m.v[11])>1e-5f||std::abs(m.v[15]-1)>1e-4f)return false;
    const auto center=scene::transformPoint(m,source.center()),e=(source.maximum-source.minimum)*.5f;
    const scene::Vec3 extent{std::abs(m.v[0])*e.x+std::abs(m.v[4])*e.y+std::abs(m.v[8])*e.z,
        std::abs(m.v[1])*e.x+std::abs(m.v[5])*e.y+std::abs(m.v[9])*e.z,
        std::abs(m.v[2])*e.x+std::abs(m.v[6])*e.y+std::abs(m.v[10])*e.z};
    if(!finite(center)||!finite(extent))return false;
    const scene::Vec3 epsilon{.01f,.01f,.01f};
    include(result,center-extent-epsilon);include(result,center+extent+epsilon);return true;
}
struct MeshBounds {
    struct Influence {std::size_t bone{};scene::Bounds bounds;};
    std::vector<Influence> influences;
    scene::Bounds local;
    bool trustworthy{true};
    static MeshBounds build(const scene::Mesh& mesh,std::size_t boneCount){
        MeshBounds result;
        std::vector<scene::Bounds> bones(mesh.skinned?boneCount:0);
        for(const auto& v:mesh.vertices){
            if(!finite(v.position)){result.trustworthy=false;break;}
            include(result.local,v.position);
            if(!mesh.skinned)continue;
            float sum=0;
            for(std::size_t j=0;j<4;++j){
                const float w=v.weights[j];sum+=w;
                if(!std::isfinite(w)||w<0||v.bones[j]>=boneCount){result.trustworthy=false;continue;}
                if(w>0)include(bones[v.bones[j]],v.position);
            }
            if(!std::isfinite(sum)||std::abs(sum-1)>1e-4f)result.trustworthy=false;
        }
        for(std::size_t i=0;i<bones.size();++i)if(bones[i].valid)result.influences.push_back({i,bones[i]});
        return result;
    }
    // For positive normalized weights, a skinned point is in the convex hull of
    // its influenced points. Their union AABB is conservative, including scales.
    bool append(scene::Bounds& result,const scene::Skeleton& skeleton,const std::vector<scene::Mat4>& pose,const scene::Mat4& model,bool skinned)const{
        if(!trustworthy)return false;
        if(!skinned)return transform(result,local,model);
        if(influences.empty())return false;
        for(const auto& part:influences){
            if(part.bone>=pose.size()||part.bone>=skeleton.bones.size()||
                !transform(result,part.bounds,model*pose[part.bone]*skeleton.bones[part.bone].inverseBind))return false;
        }
        return true;
    }
};
inline scene::Frustum paddedFrustum(scene::Mat4 vp,float pixels,int width,int height){
    const float x=width/std::max(float(width)+2*pixels,1.f),y=height/std::max(float(height)+2*pixels,1.f);
    for(int c=0;c<4;++c){vp.v[c*4]*=x;vp.v[c*4+1]*=y;}
    return scene::extractFrustum(vp);
}
inline bool visible(const scene::Bounds& bounds,const scene::Frustum& frustum){
    return !bounds.valid||scene::isAabbInFrustum(frustum,bounds.minimum,bounds.maximum);
}
}
