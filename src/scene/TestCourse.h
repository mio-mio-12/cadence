#pragma once

#include "scene/Math3D.h"

#include <algorithm>
#include <array>

namespace scene::course {

constexpr float kWorldScale=2.54f;
constexpr float scaled(float value){return value*kWorldScale;}
constexpr float kHalfExtent=scaled(2048.0f);
constexpr float kPlayerRadius=scaled(15.0f);
constexpr float kStepHeight=scaled(18.0f);

struct Box { float minX,minY,maxX,maxY,height; };
inline constexpr std::array<Box,4> kBoxes{{
    {scaled(240),scaled(-64),scaled(320),scaled(64),scaled(72)},
    {scaled(-320),scaled(-220),scaled(-220),scaled(-120),scaled(36)},
    {scaled(-100),scaled(260),scaled(360),scaled(292),scaled(96)},
    {scaled(384),scaled(-352),scaled(576),scaled(-192),scaled(64)}
}};

inline bool inside(float x,float y,const Box& box,float margin=0){return x>=box.minX-margin&&x<=box.maxX+margin&&y>=box.minY-margin&&y<=box.maxY+margin;}
inline bool onRamp(float x,float y){return x>=scaled(128)&&x<=scaled(384)&&y>=scaled(-352)&&y<=scaled(-192);}
inline float rampHeight(float x){return std::clamp((x-scaled(128.0f))/scaled(256.0f),0.0f,1.0f)*scaled(64.0f);}
inline float groundHeight(float x,float y){
    float height=onRamp(x,y)?rampHeight(x):0.0f;
    for(const auto& box:kBoxes)if(inside(x,y,box))height=std::max(height,box.height);
    return height;
}

inline Vec3 constrainMove(Vec3 oldPosition,Vec3 proposed){
    proposed.x=std::clamp(proposed.x,-kHalfExtent+kPlayerRadius,kHalfExtent-kPlayerRadius);
    proposed.y=std::clamp(proposed.y,-kHalfExtent+kPlayerRadius,kHalfExtent-kPlayerRadius);
    const float oldGround=groundHeight(oldPosition.x,oldPosition.y);
    const auto blocked=[&](float x,float y){constexpr float epsilon=scaled(0.001f);for(const auto& box:kBoxes){const bool penetrates=x>box.minX-kPlayerRadius+epsilon&&x<box.maxX+kPlayerRadius-epsilon&&y>box.minY-kPlayerRadius+epsilon&&y<box.maxY+kPlayerRadius-epsilon;if(penetrates&&proposed.z<box.height&&box.height-oldGround>kStepHeight)return true;}const float nextGround=groundHeight(x,y);return proposed.z<nextGround&&nextGround-oldGround>kStepHeight;};
    // Resolve axes independently so the capsule cannot penetrate a box but
    // still slides cleanly along faces and around corners.
    if(blocked(proposed.x,oldPosition.y))proposed.x=oldPosition.x;
    if(blocked(proposed.x,proposed.y))proposed.y=oldPosition.y;
    return proposed;
}

} // namespace scene::course
