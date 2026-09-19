#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace gameplay::bot {

enum class SpMovingReaction { None, Slip, MovingPain, NearMiss };

// Keep one per bot. Advance in GAME seconds, never unscaled render seconds.
// No backlog is emitted after a hitch; the fractional cadence is retained.
struct SpVarietyClock {
    double elapsed{};
    std::uint64_t opportunitySequence{};

    bool advance(double gameDelta,double interval) {
        if(!std::isfinite(gameDelta)||gameDelta<=0||!std::isfinite(interval)||interval<=0)return false;
        elapsed+=gameDelta;
        if(elapsed+interval*1e-10<interval)return false;
        elapsed=std::fmod(elapsed,interval);
        // Roundoff just below the boundary must not emit a second opportunity.
        if(elapsed>interval*(1.0-1e-10))elapsed=0;
        ++opportunitySequence;
        return true;
    }
};

inline bool spOpportunityChance(float probability,std::uint32_t actorId,std::uint64_t opportunitySequence,std::uint32_t salt=0) {
    if(!std::isfinite(probability)||probability<=0)return false;
    if(probability>=1)return true;
    std::uint64_t value=opportunitySequence^(static_cast<std::uint64_t>(actorId)<<32)^salt;
    value+=0x9e3779b97f4a7c15ull;
    value=(value^(value>>30))*0xbf58476d1ce4e5b9ull;
    value=(value^(value>>27))*0x94d049bb133111ebull;
    value^=value>>31;
    const double roll=static_cast<double>(value>>11)*(1.0/9007199254740992.0);
    return roll<static_cast<double>(probability);
}

struct SpMovingReactionContext {
    float gameDelta{};
    bool alive{true},grounded{true},standing{true},rifle{true};
    bool moving{},targetVisible{},mantling{},reloading{},firing{},equipment{},melee{},scenarioActive{};
    // Caller must derive these from ACTUAL damage/nearby-shot events, not mere
    // player visibility or random rolls. Absent events remain infinite.
    float actualDamageAge{std::numeric_limits<float>::infinity()};
    float actualNearbyShotAge{std::numeric_limits<float>::infinity()};
};

struct SpMovingReactionChances { float slip{0.1f},movingPain{0.65f},nearMiss{0.35f}; };

inline SpMovingReaction chooseSpMovingReaction(const SpMovingReactionContext& context,const SpMovingReactionChances& chances,std::uint32_t actorId,std::uint64_t opportunitySequence) {
    if(!std::isfinite(context.gameDelta)||context.gameDelta<=0||!context.alive||!context.grounded||!context.standing||!context.rifle||!context.moving||context.mantling||context.reloading||context.firing||context.equipment||context.melee||context.scenarioActive)return SpMovingReaction::None;
    const auto recent=[](float age){return std::isfinite(age)&&age>=0&&age<=0.35f;};
    if(recent(context.actualDamageAge))return spOpportunityChance(chances.movingPain,actorId,opportunitySequence,0x5041494eu)?SpMovingReaction::MovingPain:SpMovingReaction::None;
    if(recent(context.actualNearbyShotAge))return spOpportunityChance(chances.nearMiss,actorId,opportunitySequence,0x4e454152u)?SpMovingReaction::NearMiss:SpMovingReaction::None;
    if(!context.targetVisible&&spOpportunityChance(chances.slip,actorId,opportunitySequence,0x534c4950u))return SpMovingReaction::Slip;
    return SpMovingReaction::None;
}

// Both speeds must use the same units. Invalid/unmeasured authored root speed
// must not alter playback; modest bounds prevent extreme foot-cycle distortion.
inline float spLocomotionPlaybackRate(float actualSpeed,float authoredSpeed,float minimum=0.65f,float maximum=1.45f) {
    if(!std::isfinite(actualSpeed)||actualSpeed<0||!std::isfinite(authoredSpeed)||authoredSpeed<=1e-4f)return 1;
    if(!std::isfinite(minimum)||!std::isfinite(maximum)||minimum<=0||maximum<minimum)return 1;
    return std::clamp(actualSpeed/authoredSpeed,minimum,maximum);
}

} // namespace gameplay::bot
