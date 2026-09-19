#pragma once
#include "gameplay/ResponseCurve.h"
namespace gameplay::view {
inline float zoomDuration(float overrideSeconds,float weaponSeconds){return std::isfinite(overrideSeconds)&&overrideSeconds>0?std::clamp(overrideSeconds,.001f,5.f):std::max(.001f,weaponSeconds);}
struct ZoomResponseState {
    bool aiming{};
    float value{},from{},elapsed{};
    void update(bool target,float dt,float duration,const ResponseCurve& in,const ResponseCurve& out){
        if(target!=aiming){aiming=target;from=value;elapsed=0;}
        elapsed+=std::max(0.f,dt);
        const float t=std::clamp(elapsed/std::max(.001f,duration),0.f,1.f);
        const float amount=std::clamp((aiming?in:out).sample(t),0.f,1.f);
        value=t>=1?(aiming?1.f:0.f):from+((aiming?1.f:0.f)-from)*amount;
    }
};
}
