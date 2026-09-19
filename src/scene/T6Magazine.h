#pragma once
#include "scene/Math3D.h"
#include <optional>
#include <string_view>

namespace scene {
// Reconstructed in exported model units from magazine/hand contact across
// native reload frames, not the release or bolt position. See V75_MAGAZINES.md.
// Intentionally limited to measured view assets; never applies to CS2/worlds.
inline std::optional<Vec3> t6MagazineMount(std::string_view model) {
    if(model.starts_with("t6_wpn_ar_an94_view"))return Vec3{11.3625f,-2.75643f,-3.8091f};
    if(model.starts_with("t6_wpn_ar_scarh_view"))return Vec3{9.305f,0.13041f,-0.77438f};
    if(model.starts_with("t6_wpn_sniper_ballista_view"))return Vec3{14.7413f,0.0f,-1.0f};
    return std::nullopt;
}
}
