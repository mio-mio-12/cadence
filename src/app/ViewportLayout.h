#pragma once
#include <algorithm>
#include <cmath>
namespace cadence {
struct ViewportLayout {float width{},height{},left{},top{};int renderWidth{},renderHeight{};};
inline ViewportLayout viewportLayout(float width,float height,bool lock,float aspect,float scale,bool exporting,int exportWidth,int exportHeight){
    width=std::isfinite(width)?std::max(1.f,width):1.f;height=std::isfinite(height)?std::max(1.f,height):1.f;
    aspect=std::isfinite(aspect)?std::clamp(aspect,.5f,3.6f):16.f/9.f;scale=std::isfinite(scale)?std::clamp(scale,.25f,1.5f):1.f;
    ViewportLayout r{width,height};
    if(lock&&!exporting){if(width>height*aspect)r.width=height*aspect;else r.height=width/aspect;r.left=(width-r.width)*.5f;r.top=(height-r.height)*.5f;}
    r.renderWidth=exporting?std::max(1,exportWidth):std::max(1,int(r.width*scale));
    r.renderHeight=exporting?std::max(1,exportHeight):std::max(1,int(r.height*scale));return r;
}
}
