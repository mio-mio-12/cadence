#pragma once

#include "scene/CastScene.h"

#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace cadence::prepared_animation {

inline void appendBytes(std::string& key, const void* data, std::size_t size) {
    key.append(static_cast<const char*>(data), size);
}

template <typename T>
inline void appendValue(std::string& key, const T& value) {
    appendBytes(key, &value, sizeof(value));
}

inline std::string skeletonKey(std::string_view libraryName, const scene::Skeleton& skeleton) {
    std::string key;
    key.reserve(libraryName.size() + skeleton.bones.size() * 96);
    key.append(libraryName);
    key.push_back('\0');
    const auto boneCount = skeleton.bones.size();
    appendValue(key, boneCount);
    for (const auto& bone : skeleton.bones) {
        const auto nameLength = bone.name.size();
        appendValue(key, nameLength);
        key.append(bone.name);
        appendValue(key, bone.parent);
        appendValue(key, bone.translationTracksAreDeltas);
        appendValue(key, bone.absoluteTranslationOffset.x);
        appendValue(key, bone.absoluteTranslationOffset.y);
        appendValue(key, bone.absoluteTranslationOffset.z);
        appendValue(key, bone.restLocal.position.x);
        appendValue(key, bone.restLocal.position.y);
        appendValue(key, bone.restLocal.position.z);
        appendValue(key, bone.restLocal.rotation.x);
        appendValue(key, bone.restLocal.rotation.y);
        appendValue(key, bone.restLocal.rotation.z);
        appendValue(key, bone.restLocal.rotation.w);
        appendValue(key, bone.restLocal.scale.x);
        appendValue(key, bone.restLocal.scale.y);
        appendValue(key, bone.restLocal.scale.z);
    }
    return key;
}

struct Library {
    std::vector<scene::Animation> animations;
    std::vector<std::string> warnings;
};

class RequestCache {
public:
    [[nodiscard]] const Library* find(std::string_view libraryName, const scene::Skeleton& skeleton) const {
        const auto found = libraries_.find(skeletonKey(libraryName, skeleton));
        return found == libraries_.end() ? nullptr : &found->second;
    }

    void remember(std::string_view libraryName, const scene::Skeleton& skeleton,
                  const scene::CastScene& scene, std::size_t firstAnimation,
                  std::size_t firstWarning) {
        Library prepared;
        if (firstAnimation < scene.animations.size())
            prepared.animations.assign(scene.animations.begin() + static_cast<std::ptrdiff_t>(firstAnimation), scene.animations.end());
        if (firstWarning < scene.warnings.size())
            prepared.warnings.assign(scene.warnings.begin() + static_cast<std::ptrdiff_t>(firstWarning), scene.warnings.end());
        libraries_.insert_or_assign(skeletonKey(libraryName, skeleton), std::move(prepared));
    }

    static void append(const Library& library, scene::CastScene& scene) {
        scene.animations.insert(scene.animations.end(), library.animations.begin(), library.animations.end());
        scene.warnings.insert(scene.warnings.end(), library.warnings.begin(), library.warnings.end());
    }

    [[nodiscard]] std::size_t size() const { return libraries_.size(); }
    void clear() { libraries_.clear(); }

private:
    std::unordered_map<std::string, Library> libraries_;
};

} // namespace cadence::prepared_animation
