#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
namespace gameplay::score {
struct EffectTap {float x{},y{},weight{};};
// Gaussian glyph convolution in design pixels. Cache the kernel, not the
// score/image, so changing text, fonts, scaling or rotation remains immediate.
inline std::vector<EffectTap> effectKernel(float softness){
    const float radius=std::isfinite(softness)?std::clamp(softness,0.f,64.f):0;
    if(radius<.01f)return {{0,0,1}};
    const int steps=std::clamp(int(std::ceil(radius)),1,12);
    const float spacing=radius/steps,sigma=std::max(.2f,radius*.4f);float sum{};
    std::vector<EffectTap> taps;taps.reserve((steps*2+1)*(steps*2+1));
    for(int y=-steps;y<=steps;++y)for(int x=-steps;x<=steps;++x){
        const float px=x*spacing,py=y*spacing;if(px*px+py*py>radius*radius)continue;
        const float w=std::exp(-(px*px+py*py)/(2*sigma*sigma));taps.push_back({px,py,w});sum+=w;
    }
    for(auto& tap:taps)tap.weight/=sum;return taps;
}
struct EffectKernelCache {
    float softness{-1};std::vector<EffectTap> taps;
    const std::vector<EffectTap>& get(float value){if(value!=softness){softness=value;taps=effectKernel(value);}return taps;}
};
}
