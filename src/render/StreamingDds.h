#pragma once
#include "PreparedTexture.h"
#include <array>
#include <fstream>
#include <functional>
#include <algorithm>

namespace render::texture {
// Large, unpacked RGB atlases need neither a full-file copy nor a full-size
// RGBA intermediate. Keep the legacy repeated 2x2 integer filter exactly.
inline bool prepareRgbDds(const Request& r, Prepared& out) {
    std::ifstream in(r.path,std::ios::binary|std::ios::ate);
    const auto length=in.tellg();if(length<128)return false;in.seekg(0);
    std::array<unsigned char,128> h{};if(!in.read(reinterpret_cast<char*>(h.data()),h.size()))return false;
    const auto u=[&](int i){return unsigned(h[i])|(unsigned(h[i+1])<<8)|(unsigned(h[i+2])<<16)|(unsigned(h[i+3])<<24);};
    if(u(0)!=0x20534444||u(4)!=124||u(76)!=32||(u(80)&4)||!(u(80)&64)||u(88)!=24||u(104)!=0||u(112)!=0||u(24)>1)return false;
    const bool bgr=u(92)==0xff0000&&u(96)==0xff00&&u(100)==0xff;
    if(!bgr&&!(u(92)==0xff&&u(96)==0xff00&&u(100)==0xff0000))return false;
    const unsigned w=u(16),height=u(12);
    if(!w||!height||w>unsigned(r.maxGpuDimension)||height>unsigned(r.maxGpuDimension)||w>65536||height>65536)return false;
    if((u(8)&8)&&u(20)!=w*3)return false; // unfamiliar row pitch: retain general loader
    if(std::uint64_t(length)<128ull+std::uint64_t(w)*height*3)return false;
    std::vector<unsigned> widths{w},heights{height};
    while(widths.back()>unsigned(r.maxDimension)||heights.back()>unsigned(r.maxDimension)){
        widths.push_back(std::max(1u,widths.back()/2));heights.push_back(std::max(1u,heights.back()/2));
    }
    const auto pixels=std::uint64_t(widths.back())*heights.back()*4;
    if(pixels+std::uint64_t(w)*32>preparationBufferBudget)return false;
    std::vector<unsigned char> raw(w*3);
    std::function<std::vector<unsigned char>(size_t,unsigned)> row=[&](size_t level,unsigned y){
        std::vector<unsigned char> result(widths[level]*4,255);
        if(!level){
            in.seekg(128ull+std::uint64_t(y)*w*3);
            if(!in.read(reinterpret_cast<char*>(raw.data()),raw.size()))throw std::runtime_error("Truncated DDS row");
            for(unsigned x=0;x<w;++x){result[x*4]=raw[x*3+(bgr?2:0)];result[x*4+1]=raw[x*3+1];result[x*4+2]=raw[x*3+(bgr?0:2)];}
        }else{
            auto a=row(level-1,y*2),b=row(level-1,std::min(heights[level-1]-1,y*2+1));
            for(unsigned x=0;x<widths[level];++x)for(unsigned c=0;c<3;++c){
                const auto l=x*8+c,rr=std::min(widths[level-1]-1,x*2+1)*4+c;
                result[x*4+c]=static_cast<unsigned char>((unsigned(a[l])+a[rr]+b[l]+b[rr]+2)/4);
            }
        }
        return result;
    };
    out.bytes.resize(static_cast<size_t>(pixels));
    for(unsigned y=0;y<heights.back();++y){auto pixelsRow=row(widths.size()-1,y);std::copy(pixelsRow.begin(),pixelsRow.end(),out.bytes.begin()+size_t(y)*widths.back()*4);}
    out.width=widths.back();out.height=heights.back();out.internalFormat=r.compress?(r.srgb?0x8C4C:0x83F0):(r.srgb?0x8C43:0x8058);
    out.ready=true;return true;
}
}
