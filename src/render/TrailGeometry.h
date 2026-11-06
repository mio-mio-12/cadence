#pragma once
#include "scene/Math3D.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace render::trails {
struct Interval { float tail{}, head{}; bool visible{}; };
// Fixed exposure, independent of playback FPS. The first frame includes the
// barrel even when the projectile travels more than its length in one frame.
inline Interval interval(float distance,float progress,float speed,float length) {
    if(!std::isfinite(distance)||!std::isfinite(progress)||!std::isfinite(speed)||!std::isfinite(length)||distance<=0)return {};
    const float tail=std::max(0.f,progress-std::max(0.f,length));
    const float head=std::min(distance,std::max(0.f,progress)+std::max(0.f,speed)/120.f);
    return {tail,head,head>tail};
}
inline float smooth(float t) { t=std::clamp(t,0.f,1.f);return t*t*(3.f-2.f*t); }
inline float profile(float across,float feather) {
    const float d=std::abs(across);
    const float f=std::clamp(feather,.001f,1.f);
    return 1.f-smooth((d-(1.f-f))/f);
}
// Feathering is baked into the ribbon, so different shots can share a draw
// without losing their individual feather setting to a batch-wide uniform.
template<class Width>
inline void ribbon(std::vector<float>& out,scene::Vec3 a,scene::Vec3 b,scene::Vec3 side,
                   scene::Vec3 color,float opacity,float feather,bool sprite,Width width) {
    constexpr int segments=16;
    const int bands=sprite?1:8;
    constexpr int indices[]{0,1,2,0,2,3};
    for(int s=0;s<segments;++s) {
        const float u0=float(s)/segments,u1=float(s+1)/segments;
        const auto p0=a+(b-a)*u0,p1=a+(b-a)*u1;
        const float w0=std::max(0.f,width(u0)),w1=std::max(0.f,width(u1));
        for(int band=0;band<bands;++band) {
            const float v0=float(band)/bands,v1=float(band+1)/bands;
            const float x0=2*v0-1,x1=2*v1-1;
            const scene::Vec3 p[]{p0+side*(w0*x0),p1+side*(w1*x0),p1+side*(w1*x1),p0+side*(w0*x1)};
            const float uv[][2]{{u0,v0},{u1,v0},{u1,v1},{u0,v1}};
            for(int i:indices) {
                const float coverage=sprite?1.f:profile(uv[i][1]*2-1,feather);
                out.insert(out.end(),{p[i].x,p[i].y,p[i].z,uv[i][0],uv[i][1],color.x,color.y,color.z,opacity*coverage});
            }
        }
    }
}
}
