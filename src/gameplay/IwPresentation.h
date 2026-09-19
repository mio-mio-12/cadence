#pragma once

#include "scene/CastScene.h"

#include <algorithm>
#include <cmath>

namespace gameplay::iw {

// Saluki preserves model coordinates in centimetres while classic IW tuning
// is authored in inches. Models remain untouched; gameplay crosses this one
// explicit inch-to-centimetre boundary instead of scaling any CAST skeleton.
constexpr float kWorldUnitsPerIwUnit = 2.54f;
constexpr float worldUnits(float iwUnits) { return iwUnits*kWorldUnitsPerIwUnit; }
constexpr float kRunSpeed = worldUnits(190.0f);
constexpr float kSprintScale = 1.5f;
constexpr float kBackSpeedScale = 0.7f;
constexpr float kStrafeSpeedScale = 0.8f;
constexpr float kSprintStrafeSpeedScale = 0.667f;
constexpr float kStopSpeed = worldUnits(100.0f);
constexpr float kFriction = 5.5f;
constexpr float kAirAccelerate = 1.0f;
constexpr float kGravity = worldUnits(800.0f);
constexpr float kJumpHeight = worldUnits(39.0f);
constexpr float kMaximumViewBob = worldUnits(8.0f);
constexpr float kUnitsToMeters = 0.01f;
constexpr float kMetersToUnits = 1.0f/kUnitsToMeters;
constexpr float kStandingActorHeight = worldUnits(72.0f);

inline float viewHeight(scene::Stance stance) {
    if (stance == scene::Stance::Prone) return worldUnits(11.0f);
    if (stance == scene::Stance::Crouch) return worldUnits(40.0f);
    return worldUnits(60.0f);
}

inline float stanceSpeedScale(scene::Stance stance) {
    if (stance == scene::Stance::Prone) return 0.15f;
    if (stance == scene::Stance::Crouch) return 0.65f;
    return 1.0f;
}

inline float groundAcceleration(scene::Stance stance) {
    if (stance == scene::Stance::Prone) return 19.0f;
    if (stance == scene::Stance::Crouch) return 12.0f;
    return 9.0f;
}

inline float stanceTransitionTime(scene::Stance from, scene::Stance to) {
    if (from == to) return 0.0f;
    if (from == scene::Stance::Prone || to == scene::Stance::Prone) return 0.40f;
    return 0.20f;
}

struct ViewBob {
    float horizontal{};
    float vertical{};
};

inline float bobMove(scene::Stance stance, bool sprinting) {
    if (sprinting) return 0.70f;
    if (stance == scene::Stance::Prone) return 0.30f;
    if (stance == scene::Stance::Crouch) return 0.50f;
    return 0.40f;
}

inline void advanceBobCycle(float& radians, float deltaSeconds, scene::Stance stance, bool sprinting) {
    constexpr float twoPi = scene::kPi * 2.0f;
    radians = std::fmod(radians + deltaSeconds * bobMove(stance, sprinting) * (1000.0f / 255.0f) * twoPi, twoPi);
}

inline ViewBob sampleViewBob(float cycle, float horizontalSpeed, scene::Stance stance, bool sprinting) {
    float horizontalAmplitude = 0.007f;
    float verticalAmplitude = 0.007f;
    if (sprinting) {
        horizontalAmplitude = 0.020f;
        verticalAmplitude = 0.014f;
    } else if (stance == scene::Stance::Prone) {
        horizontalAmplitude = 0.020f;
        verticalAmplitude = 0.005f;
    } else if (stance == scene::Stance::Crouch) {
        horizontalAmplitude = 0.0075f;
        verticalAmplitude = 0.0075f;
    }
    ViewBob result;
    result.horizontal = std::min(kMaximumViewBob, horizontalSpeed * horizontalAmplitude) * std::sin(cycle);
    result.vertical = std::min(kMaximumViewBob, horizontalSpeed * verticalAmplitude) * 0.75f *
        (std::sin(cycle * 2.0f) + 0.2f * std::sin(cycle * 4.0f + scene::kPi * 0.5f));
    return result;
}

inline float landingDip(float elapsed, float duration, float amplitude) {
    if (duration <= 0.0f || elapsed >= duration) return 0.0f;
    // id Tech/IW-style land response: reach the deflected position quickly,
    // then return more slowly.  The classic split is 150 ms / 300 ms.
    const float deflect = duration / 3.0f;
    if (elapsed < deflect) return -amplitude * (elapsed / deflect);
    return -amplitude * (1.0f - (elapsed - deflect) / std::max(0.001f, duration - deflect));
}

} // namespace gameplay::iw
