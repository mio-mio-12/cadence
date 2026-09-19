#pragma once

#include "take/Take.h"

#include <vector>

namespace take {

// Matches IWXMVM's campath channel behavior: linear below four nodes,
// Numerical Recipes cubic spline at four or more nodes.
[[nodiscard]] DollyCameraKeyframe interpolateDollyCamera(const std::vector<DollyCameraKeyframe>& keyframes,float tick);

} // namespace take
