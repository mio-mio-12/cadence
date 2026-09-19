#pragma once
#include "scene/CastScene.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace gameplay {
// A camera-space rigid motion, never a replacement pose. Applying the same
// matrix to every skin/attachment preserves inverse(hand) * weapon exactly.
struct SprintMotion {
    std::vector<scene::Transform> samples;
    float duration{};
    scene::Mat4 sample(float seconds, float weight) const {
        if(samples.empty() || duration<=0 || weight<=0)return scene::Mat4::identity();
        const float phase=std::fmod(std::max(0.0f,seconds),duration)/duration*samples.size();
        const auto index=static_cast<std::size_t>(phase)%samples.size();
        const auto& a=samples[index];const auto& b=samples[(index+1)%samples.size()];
        const float t=phase-std::floor(phase),w=std::clamp(weight,0.0f,1.0f);
        return scene::trs(scene::lerp(a.position,b.position,t)*w,
            scene::slerp({0,0,0,1},scene::slerp(a.rotation,b.rotation,t),w),{1,1,1});
    }
};
inline float sprintEnvelope(float weight,bool active,float delta,float seconds){
    return weight+std::clamp((active?1.0f:0.0f)-weight,-std::max(0.0f,delta)/std::max(.01f,seconds),std::max(0.0f,delta)/std::max(.01f,seconds));
}
}
