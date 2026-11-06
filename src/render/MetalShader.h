#pragma once
#include "scene/Math3D.h"
#include <tuple>
namespace render::metal {
struct Settings {
    int mode{}; // Current, Skybox metal, BO1 M1911.
    float reflection{1},roughness{.08f},normal{1},reflectance{.9f},coverage{1};
    float direct{1},diffuse{.03f},bo1RoughnessScale{1};
    scene::Vec3 tint{1,1,1};
    void sanitize(){
        const Settings defaults;mode=std::clamp(mode,0,2);
        const auto value=[](float& v,float fallback,float low,float high){v=std::isfinite(v)?std::clamp(v,low,high):fallback;};
        value(reflection,defaults.reflection,0,100);value(roughness,defaults.roughness,0,1);value(normal,defaults.normal,0,4);
        value(reflectance,defaults.reflectance,0,1);value(coverage,defaults.coverage,0,1);value(direct,defaults.direct,0,100);
        value(diffuse,defaults.diffuse,0,1);value(bo1RoughnessScale,defaults.bo1RoughnessScale,0,10);
        value(tint.x,1,0,1);value(tint.y,1,0,1);value(tint.z,1,0,1);
    }
};
template<class T> auto values(T& s){return std::tie(s.mode,s.reflection,s.roughness,s.normal,s.reflectance,s.coverage,s.direct,s.diffuse,s.bo1RoughnessScale,s.tint.x,s.tint.y,s.tint.z);}
}
