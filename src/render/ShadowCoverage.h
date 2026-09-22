#pragma once
#include "scene/CastScene.h"
namespace render::shadow {
struct Coverage { scene::Mat4 matrix; float biasScale{1}; };
inline Coverage coverage(scene::Vec3 center,scene::Vec3 direction,float radius,const scene::Bounds& bounds){
    direction=scene::normalize(direction);
    const auto up=std::abs(direction.z)>.95f?scene::Vec3{0,1,0}:scene::Vec3{0,0,1};
    float upstream=radius*1.8f;
    if(bounds.valid)for(int i=0;i<8;++i){
        const scene::Vec3 p{i&1?bounds.maximum.x:bounds.minimum.x,i&2?bounds.maximum.y:bounds.minimum.y,i&4?bounds.maximum.z:bounds.minimum.z};
        upstream=std::max(upstream,-scene::dot(p-center,direction)+10.f);
    }
    const float farPlane=upstream+radius*2.2f;
    return {scene::orthographic(-radius,radius,-radius,radius,1,farPlane)*scene::lookAt(center-direction*upstream,center,up),(radius*4.f-1.f)/(farPlane-1.f)};
}
}
