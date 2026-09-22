#pragma once
#include <cmath>
#include <istream>
#include <ostream>
namespace render::rain {
struct Style {
    float rate{2.953f},lifetime{.21f},jitter{2},fade{3.656f};
    float ringOpacity{.151f},ringWidth{.001f},expansion{1.261f},crownHeight{.863f},crownSpread{1.726f},dropSize{.394f},dropCount{5};
    float grainScale{201.630f},grainAmount{.955f},grainContrast{1.101f};
    bool refractive{};float refractionMix{.7f},ior{1.333f},roughness{.05f},refractionStrength{4};
    template<class F> void visit(F f){f(rate);f(lifetime);f(jitter);f(fade);f(ringOpacity);f(ringWidth);f(expansion);f(crownHeight);f(crownSpread);f(dropSize);f(dropCount);f(grainScale);f(grainAmount);f(grainContrast);f(refractive);f(refractionMix);f(ior);f(roughness);f(refractionStrength);}
};
inline std::ostream& operator<<(std::ostream& out,Style s){s.visit([&](auto v){out<<v<<' ';});return out;}
inline std::istream& operator>>(std::istream& in,Style& s){Style p;p.visit([&](auto& v){in>>v;if(!std::isfinite(double(v)))in.setstate(std::ios::failbit);});if(in)s=p;return in;}
}
