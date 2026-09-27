#pragma once
#include <algorithm>

namespace render::rain {
// Exact reuse only: the projection and included mesh set must be unchanged.
// Kept separately for rain and weather, and subordinate to their tile/wind keys.
struct ShelterCeilingReuse {
    float lower{1e30f}, upper{-1e30f};
    void reset() { lower=1e30f; upper=-1e30f; }
    void set(float geometryTop,float nextExcludedBottom,bool nonempty,float ceiling) {
        reset();
        if(nonempty && ceiling>=geometryTop+10.f){lower=geometryTop+10.f;upper=nextExcludedBottom;}
    }
    bool contains(float ceiling) const { return ceiling>=lower && ceiling<upper; }
};
}
