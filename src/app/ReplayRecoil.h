#pragma once
#include "take/Take.h"
#include "weapon/WeaponProfile.h"

namespace cadence::replay {
// Presentation only: seekable shot impulses, no mutable simulation state and
// no take-format changes. Settings come from the currently selected weapon.
inline scene::Vec3 recoilAt(const take::Take& recording,float time,std::uint8_t slot,const weapon::Stats& settings){
    if(!std::isfinite(time)||recording.samples.empty()||recording.shots.empty())return {};
    auto shape=settings.recoil;shape.sanitize();
    if(shape.intensity<=0)return {};
    const auto finite=[](float value,float fallback){return std::isfinite(value)?value:fallback;};
    const auto duration=[&](float speed){return shape.duration*std::clamp(15.f/std::max(1.f,finite(speed,15.f)),.25f,4.f);};
    const float hipDuration=duration(settings.hipKickCenterSpeed),adsDuration=duration(settings.adsKickCenterSpeed);
    const float oldest=time-std::max(hipDuration,adsDuration);
    auto end=std::upper_bound(recording.shots.begin(),recording.shots.end(),time,[](float t,const take::ShotEvent& shot){return t<shot.time;});
    auto begin=end;
    // Match the live response's bounded 64-pulse capacity. Only recent events
    // are visited, never the entire demo, even at very low playback speeds.
    for(int n=0;n<64&&begin!=recording.shots.begin();++n){auto previous=begin-1;if(previous->time<oldest)break;begin=previous;}
    scene::Vec3 sum{};
    for(auto it=begin;it!=end;++it){
        const auto* fired=recording.sampleAt(it->time);
        if(!fired||fired->weaponSlot!=slot)continue; // Do not borrow another weapon's shots.
        const bool aimed=fired->camera.adsBlend>=.5f;
        const float age=time-it->time,pulseDuration=aimed?adsDuration:hipDuration;
        if(age<0||age>=pulseDuration)continue;
        const auto sequence=static_cast<std::size_t>(it-recording.shots.begin())+1;
        const float phase=static_cast<float>(std::fmod(double(sequence)*.61803398875,1.));
        const auto range=[&](float low,float high,float fraction){low=std::clamp(finite(low,0),-100.f,100.f);high=std::clamp(finite(high,0),-100.f,100.f);return low+(high-low)*fraction;};
        const scene::Vec3 amplitude{
            range(aimed?settings.adsKickPitchMin:settings.hipKickPitchMin,aimed?settings.adsKickPitchMax:settings.hipKickPitchMax,phase),
            range(aimed?settings.adsKickYawMin:settings.hipKickYawMin,aimed?settings.adsKickYawMax:settings.hipKickYawMax,std::fmod(phase*1.731f,1.f)),
            range(shape.rollMin,shape.rollMax,phase)};
        sum+=amplitude*shape.sample(age/pulseDuration);
    }
    return scene::Vec3{std::clamp(sum.x,-shape.maxPitch,shape.maxPitch),std::clamp(sum.y,-shape.maxYaw,shape.maxYaw),std::clamp(sum.z,-shape.maxRoll,shape.maxRoll)}*shape.intensity;
}
}
