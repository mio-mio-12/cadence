#pragma once
#include <algorithm>
#include <limits>

namespace render::texture {
// Zero is a user-facing Original choice. Decode/upload still checks the actual
// device limit separately; never silently resize Original to the hardware cap.
inline int mapResolutionSetting(int requested) noexcept {
    return requested == 0 ? 0 : std::clamp(requested, 256, 4096);
}
inline int decodeResolutionLimit(int setting) noexcept {
    return setting == 0 ? std::numeric_limits<int>::max() : mapResolutionSetting(setting);
}
}
