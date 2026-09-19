#include "app/PreparedAnimationLibrary.h"

#include <stdexcept>

namespace {
void require(bool value) { if (!value) throw std::runtime_error("prepared animation library regression"); }

scene::Skeleton skeleton(std::string name, float offset = 0.0f) {
    scene::Skeleton result;
    scene::Bone bone;
    bone.name = std::move(name);
    bone.parent = -1;
    bone.absoluteTranslationOffset.x = offset;
    result.bones.push_back(bone);
    result.boneByName.emplace(result.bones[0].name, 0);
    result.boneByCanonicalName.emplace(result.bones[0].name, 0);
    return result;
}
}

int main() {
    cadence::prepared_animation::RequestCache cache;
    scene::CastScene first;
    first.skeleton = skeleton("tag_origin");
    first.animations.push_back(scene::Animation{.name = "base"});
    first.animations.push_back(scene::Animation{.name = "pb_run", .sourceGame = "bo2"});
    first.warnings.push_back("existing");
    first.warnings.push_back("prepared warning");
    cache.remember("bo2", first.skeleton, first, 1, 1);
    require(cache.size() == 1);

    scene::CastScene second;
    second.skeleton = first.skeleton;
    const auto* prepared = cache.find("bo2", second.skeleton);
    require(prepared && prepared->animations.size() == 1 && prepared->warnings.size() == 1);
    cadence::prepared_animation::RequestCache::append(*prepared, second);
    require(second.animations.size() == 1 && second.animations[0].name == "pb_run");
    require(second.animations[0].sourceGame == "bo2" && second.warnings[0] == "prepared warning");

    require(!cache.find("ghosts", second.skeleton));
    auto changedName = skeleton("j_mainroot");
    require(!cache.find("bo2", changedName));
    auto changedLayout = skeleton("tag_origin", 1.0f);
    require(!cache.find("bo2", changedLayout));
    return 0;
}
