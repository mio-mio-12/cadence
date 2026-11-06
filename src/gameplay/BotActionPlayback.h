#pragma once
#include "scene/CastScene.h"
#include <algorithm>
#include <cmath>

namespace gameplay::bot {
struct ActionCrossfade {
    std::size_t previous{static_cast<std::size_t>(-1)};
    float frame{},elapsed{};
    void select(std::size_t current,float currentFrame,std::size_t next,float weight){
        if(current==next)return; // Repeated shots must not restart the crossfade.
        previous=weight>.001f?current:static_cast<std::size_t>(-1);
        frame=currentFrame;elapsed=0;
    }
    float incoming(float seconds) const {
        const float t=std::clamp(elapsed/std::max(.001f,seconds),0.f,1.f);
        return t*t*(3.f-2.f*t);
    }
};
inline bool restartBotAction(bool sameClip,bool active,scene::ActionRole action,bool continuousFire,float frame,float duration){
    // Automatic/burst fire traverses the whole authored cycle while actual
    // shots keep it active. Discrete shots and each new reload restart once.
    return !sameClip||!active||action!=scene::ActionRole::Fire||!continuousFire||frame>=duration;
}
inline float advanceBotAction(float frame,float delta,float fps,float duration){
    return std::clamp(frame+std::max(0.f,delta)*std::max(0.f,fps),0.f,std::max(0.f,duration));
}
inline void configureBotOneShots(scene::CastScene& scene){
    for(auto& clip:scene.animations){
        switch(clip.action){
        case scene::ActionRole::Reload:case scene::ActionRole::Fire:case scene::ActionRole::Death:
        case scene::ActionRole::Throw:case scene::ActionRole::GrenadePrep:case scene::ActionRole::Flinch:
            clip.looping=false;break;
        default:break;
        }
    }
}
}
