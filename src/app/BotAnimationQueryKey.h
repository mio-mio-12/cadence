#pragma once
#include "scene/CastScene.h"
#include <tuple>
#include <map>

namespace bot_animation {
// Torso overrides are authored in a posture, not relative to the body's pose.
// A standing torso over prone legs produces an upright, bent-spine "cobra".
inline bool postureCompatible(const scene::Animation& clip, scene::Stance stance) {
    return clip.stance==stance || clip.stance==scene::Stance::Any || stance==scene::Stance::Any;
}
inline scene::AnimationQuery actionQuery(scene::ActionRole action,scene::WeaponClass weapon,scene::Stance stance) {
    scene::AnimationQuery query;
    query.domain=scene::AnimationDomain::PlayerTorso;
    query.action=action;query.weapon=weapon;query.stance=stance;
    // Bots have a weapon class, not a magazine-style binding. Permit the
    // available posture-specific reload instead of rejecting handle/rear clips.
    if(action==scene::ActionRole::Reload)query.reloadStyle=scene::ReloadStyle::Any;
    return query;
}
// Include every query field: equal keys must imply identical selection inputs.
inline auto queryKey(const scene::AnimationQuery& q) {
    return std::make_tuple(q.domain, q.motion, q.action, q.weapon, q.stance,
        q.direction, q.ads, q.reloadStyle, q.allowContextual,
        q.requireMappedTracks, q.preferredGame, q.forceT6Locomotion);
}
using QueryKey = decltype(queryKey(scene::AnimationQuery{}));
struct ActionCache {
    std::map<QueryKey, std::optional<std::size_t>> entries;
    const scene::CastScene* source{};
    std::size_t clipCount{}, misses{}, hits{};
    void clear() { entries.clear(); source=nullptr; clipCount=misses=hits=0; }
    std::optional<std::size_t> find(const scene::CastScene& scene, const scene::AnimationQuery& query) {
        if(source!=&scene || clipCount!=scene.animations.size()) {
            clear(); source=&scene; clipCount=scene.animations.size();
        }
        const auto key=queryKey(query);
        if(const auto found=entries.find(key); found!=entries.end()) { ++hits; return found->second; }
        ++misses;
        return entries.emplace(key,scene::findBestAnimation(scene,query)).first->second;
    }
};
}
