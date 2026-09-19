#pragma once
#include "scene/CastScene.h"
#include <algorithm>
#include <cmath>

namespace gameplay::bot {
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
