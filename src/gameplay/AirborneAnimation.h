#pragma once

#include <algorithm>
#include <cmath>

namespace gameplay::presentation {

// Updated by gameplay, never by drawing: multiple viewports/recording must not
// advance takeoff twice, and changing torso actions must not restart the legs.
struct AirborneAnimation {
    bool active{};
    float elapsed{};

    void update(bool airborne, float delta) {
        if (!airborne) { active = false; return; }
        if (!active) elapsed = 0.0f;
        else elapsed += std::max(0.0f, delta);
        active = true;
    }
};

inline float heldAnimationFrame(float frame, float duration, bool looping) {
    // CastScene wraps even an exact end frame on loop-tagged source clips.
    // Stay immediately below that boundary without modifying imported metadata.
    const float end = looping && duration > 0.0f ? std::nextafter(duration, 0.0f) : std::max(0.0f, duration);
    return std::clamp(frame, 0.0f, end);
}

inline float airborneAnimationFrame(const AirborneAnimation& state, float rate, float duration, bool looping) {
    return heldAnimationFrame(state.elapsed * std::max(0.0f, rate), duration, looping);
}

} // namespace gameplay::presentation
