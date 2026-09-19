#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
namespace cadence::depth_export {
// Frame-independent noise: quantization decorrelation without temporal flicker.
inline float noise(std::uint32_t pixel){pixel^=0x9e3779b9u;pixel^=pixel>>16;pixel*=0x7feb352du;pixel^=pixel>>15;pixel*=0x846ca68bu;pixel^=pixel>>16;return float(pixel&0xffffffu)/16777216.f-.5f;}
inline std::uint16_t quantize(float depth,std::uint32_t pixel,bool dither,float strength,bool fullRange){
    if(!std::isfinite(depth))depth=1.f;
    depth=std::clamp(depth,0.f,1.f);const int lo=fullRange?0:64,hi=fullRange?1023:940;
    // Keep background and clipping endpoints exact; never dither their masks.
    const float n=dither&&depth>0&&depth<1?noise(pixel)*std::clamp(strength,0.f,1.f):0.f;
    return static_cast<std::uint16_t>(std::clamp(int(std::lround(lo+depth*(hi-lo)+n)),lo,hi));
}
inline void pack(const std::vector<float>& depth,std::vector<std::uint16_t>& yuv,bool dither,float strength,bool fullRange){
    yuv.resize(depth.size()*3);for(std::size_t i=0;i<depth.size();++i)yuv[i]=quantize(depth[i],static_cast<std::uint32_t>(i),dither,strength,fullRange);
    std::fill(yuv.begin()+depth.size(),yuv.end(),512); // neutral chroma; no RGB/gamma conversion
}
}
