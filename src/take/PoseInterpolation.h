#pragma once
#include "scene/CastScene.h"

namespace take {
inline scene::Mat4 interpolateTransform(const scene::Mat4& a,const scene::Mat4& b,float t){
    scene::Vec3 ap,as,bp,bs;scene::Quat aq,bq;
    scene::decomposeAffine(a,ap,aq,as);scene::decomposeAffine(b,bp,bq,bs);
    return scene::trs(scene::lerp(ap,bp,t),scene::slerp(aq,bq,t),scene::lerp(as,bs,t));
}
inline bool invertiblePose(const scene::Mat4& m){
    const float determinant=m.v[0]*(m.v[5]*m.v[10]-m.v[9]*m.v[6])-m.v[4]*(m.v[1]*m.v[10]-m.v[9]*m.v[2])+m.v[8]*(m.v[1]*m.v[6]-m.v[5]*m.v[2]);
    return std::isfinite(determinant)&&std::abs(determinant)>1e-10f;
}
// Recordings retain their exact global samples. Only fractional playback uses
// parent-local interpolation, avoiding shrinking limbs on rotational arcs.
inline std::vector<scene::Mat4> interpolatePose(const std::vector<scene::Mat4>& a,const std::vector<scene::Mat4>& b,float t,const scene::Skeleton* rig){
    if(t<=0)return a;if(t>=1)return b;
    if(a.size()!=b.size())return t<.5f?a:b;
    const auto count=a.size();std::vector<scene::Mat4> result(count);
    if(!rig||rig->bones.size()!=count){for(std::size_t i=0;i<count;++i)result[i]=interpolateTransform(a[i],b[i],t);return result;}
    std::vector<unsigned char> state(count,0);
    const auto solve=[&](auto&& self,std::size_t i)->void{
        if(state[i]==2)return;
        if(state[i]==1){result[i]=interpolateTransform(a[i],b[i],t);state[i]=2;return;}
        state[i]=1;const auto parent=rig->bones[i].parent;
        if(parent>=0&&static_cast<std::size_t>(parent)<count&&static_cast<std::size_t>(parent)!=i&&state[parent]!=1&&invertiblePose(a[parent])&&invertiblePose(b[parent])){
            self(self,static_cast<std::size_t>(parent));
            result[i]=result[parent]*interpolateTransform(scene::inverseAffine(a[parent])*a[i],scene::inverseAffine(b[parent])*b[i],t);
        }else result[i]=interpolateTransform(a[i],b[i],t);
        state[i]=2;
    };
    for(std::size_t i=0;i<count;++i)solve(solve,i);
    return result;
}
} // namespace take
