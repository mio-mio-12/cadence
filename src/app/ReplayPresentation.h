#pragma once
#include "take/Take.h"
#include <cmath>

namespace cadence::replay {
inline bool pauseForInactiveWindow(bool focused,bool minimized,bool captureActive){
    return (!focused||minimized)&&!captureActive;
}
struct ClipRange{float nearPlane,farPlane;};
inline ClipRange clipRange(float nearCm,float farMeters){
    const float nearPlane=std::isfinite(nearCm)?std::max(.01f,nearCm):2.5f;
    const float farPlane=std::isfinite(farMeters)?farMeters*100.f:100000.f;
    return {nearPlane,std::max(nearPlane+1.f,farPlane)};
}
inline std::uint64_t tickAt(float time,float duration,float rate){
    return static_cast<std::uint64_t>(std::llround(std::clamp(time,0.f,std::max(0.f,duration))*std::max(1.f,rate)));
}
inline void setCaptureBoundary(float time,float duration,float rate,bool start,float& begin,float& end){
    const float snapped=std::min(duration,float(tickAt(time,duration,rate))/std::max(1.f,rate));
    begin=std::clamp(begin,0.f,duration);end=std::clamp(end,begin,duration);
    if(start){begin=snapped;end=std::max(end,begin);}else{end=snapped;begin=std::min(begin,end);}
}
// Preserve the recorded optical magnification, not a linear angle ratio.
// Only the projection changes; ADS timing, camera poses and visibility do not.
inline float presentationFov(float recordedFov,float recordedHip,float currentHip){
    const auto safe=[](float v,float fallback){return std::isfinite(v)?std::clamp(v,1.f,179.f):fallback;};
    recordedHip=safe(recordedHip,65.f);recordedFov=safe(recordedFov,recordedHip);currentHip=safe(currentHip,recordedHip);
    if(std::abs(currentHip-recordedHip)<.00001f)return recordedFov;
    const float radians=scene::kPi/360.f;
    return std::clamp(std::atan(std::tan(recordedFov*radians)*std::tan(currentHip*radians)/std::tan(recordedHip*radians))/radians,1.f,179.f);
}
inline float presentationFov(const take::Take& take,const take::Sample& sample,float fov,float scale){
    const auto& slot=take.actorSlots[std::min<std::size_t>(sample.weaponSlot,2)];
    const auto& manifest=slot.empty()?take.actor:slot;
    return presentationFov(sample.camera.fov,manifest.viewmodelFov*manifest.viewmodelFovScale,fov*scale);
}
}
