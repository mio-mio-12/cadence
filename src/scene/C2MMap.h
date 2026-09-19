#pragma once

#include "scene/GlbMap.h"

#include <filesystem>
#include <functional>
#include <string>

namespace scene::c2m {

[[nodiscard]] bool load(
    const std::filesystem::path& path,
    scene::glb::Map& map,
    std::string& error,
    float scaleMultiplier = 1.0f,
    const std::function<void(std::string_view, float)>& onProgress = nullptr,
    const LoadOptions& options = {});

} // namespace scene::c2m
