#pragma once
#include <algorithm>

namespace render::foreground_depth {
// Centimetres. Keep the view rig independent of the map's precision settings.
inline constexpr float nearPlane=.1f,farPlane=2000.f,slice=.04f;
inline double linearize(double raw,double nearValue,double farValue,bool foreground,bool separate){
    if(foreground&&raw<.0400001){
        raw=std::clamp(raw/double(slice),0.0,1.0);
        if(separate){nearValue=nearPlane;farValue=farPlane;}
    }
    return 2.0*nearValue*farValue/std::max(.0000001,farValue+nearValue-(raw*2.0-1.0)*(farValue-nearValue));
}
}
