#pragma once
#include "scene/CastScene.h"
#include <optional>
#include <string_view>

namespace scene {
struct T6MagazineCalibration { Vec3 position{}; float residual{}; std::size_t supportingWindows{}; bool incomingSocket{}; };
// Native, unretargeted reloads only. No weapon-specific offsets or file I/O.
std::optional<T6MagazineCalibration> inferT6MagazineMount(const CastScene& native);
bool applyT6MagazineMount(CastScene& target,const T6MagazineCalibration& mount);
bool hasSeparateT6Magazine(const CastScene& scene);
}
