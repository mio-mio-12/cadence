#pragma once
#include "scene/Math3D.h"
#include <algorithm>
#include <cmath>
namespace render::viewmodel_projection {
// Convert a foreground-layer point into a world-projected point with the
// same screen XY and depth. Only presentation changes; shot endpoints stay put.
inline scene::Vec3 worldOrigin(scene::Vec3 point,scene::Vec3 camera,const scene::Mat4& vp,float cameraFov,float multiplier,bool flip){
    if(!std::isfinite(multiplier)||multiplier<=0)multiplier=1.f;
    float scale=1.f;
    if(multiplier!=1.f&&std::isfinite(cameraFov)){const float source=std::clamp(cameraFov,.01f,179.f);scale=std::tan(source*scene::kPi/360.f)/std::tan(std::clamp(source*multiplier,.01f,179.f)*scene::kPi/360.f);}
    const auto right=scene::normalize(scene::Vec3{vp.v[0],vp.v[4],vp.v[8]}),up=scene::normalize(scene::Vec3{vp.v[1],vp.v[5],vp.v[9]});
    const auto offset=point-camera;
    return point+right*(scene::dot(offset,right)*((flip?-scale:scale)-1.f))+up*(scene::dot(offset,up)*(scale-1.f));
}
inline scene::Mat4 adjusted(scene::Mat4 viewProjection,float cameraFov,float multiplier,bool flip){
    if(!std::isfinite(multiplier)||multiplier<=0)multiplier=1.f;
    float scale=1.f;
    if(multiplier!=1.f&&std::isfinite(cameraFov)){
        const auto source=std::clamp(cameraFov,.01f,179.f);
        const auto target=std::clamp(source*multiplier,.01f,179.f);
        scale=std::tan(source*scene::kPi/360.f)/std::tan(target*scene::kPi/360.f);
    }
    // Clip-space XY only: no world camera, ADS zoom or depth-range change.
    for(int col=0;col<4;++col){viewProjection.v[col*4]*=flip?-scale:scale;viewProjection.v[col*4+1]*=scale;}
    return viewProjection;
}
}
