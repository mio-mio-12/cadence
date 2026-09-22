#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <istream>
#include <ostream>
#include <vector>

namespace render::water {
struct Surface {
    float tightness{.55f}, foamPersistence{1.6f}, foamDetail{.8f};
    void sanitize(){
        const auto c=[](float& x,float,float,float fallback){if(!std::isfinite(x))x=fallback;};
        c(tightness,0,1,.55f);c(foamPersistence,0,4,1.6f);c(foamDetail,0,1,.8f);
    }
};
inline std::ostream& operator<<(std::ostream& out,const Surface& p){
    return out<<p.tightness<<' '<<p.foamPersistence<<' '<<p.foamDetail;
}
inline std::istream& operator>>(std::istream& in,Surface& p){
    Surface q;
    if(in>>q.tightness>>q.foamPersistence>>q.foamDetail){
        for(float v:{q.tightness,q.foamPersistence,q.foamDetail})
            if(!std::isfinite(v)){in.setstate(std::ios::failbit);return in;}
        q.sanitize();p=q;
    }
    return in;
}
// Eulerian trochoid: phi = theta + e*cos(theta), h = sin(theta).
// e < 1 makes the inverse unique; no folded geometry or unstable Newton solve.
// Remove the spatial mean and preserve sine-wave variance (1/2). This controls
// crest width independently of the multi-wave horizontal nonfolding bound.
inline std::array<float,4> crestProfile(double phase,float tightness){
    constexpr double tau=6.2831853071795864769;
    const double e=.97*std::clamp(double(tightness),0.0,1.0);
    const double phi=std::remainder(phase,tau);
    double lo=phi-e,hi=phi+e;
    for(int i=0;i<40;++i){
        const double mid=(lo+hi)*.5;
        if(mid+e*std::cos(mid)<phi)lo=mid;else hi=mid;
    }
    const double theta=(lo+hi)*.5,s=std::sin(theta),c=std::cos(theta);
    const double norm=1/std::sqrt(1-e*e*.5);
    return {float((s+e*.5)*norm),float(c/(1-e*s)*norm),float(std::max(0.0,s)),1.f};
}
inline constexpr int profileSize=2048,foamTextureSize=512;
inline std::vector<float> crestTable(float tightness){
    std::vector<float> data(profileSize*4);
    for(int i=0;i<profileSize;++i){
        const auto p=crestProfile((i+.5)*6.2831853071795864769/profileSize,tightness);
        std::copy(p.begin(),p.end(),data.begin()+i*4);
    }
    return data;
}
inline std::uint32_t foamHash(int x,int y,int period){
    x=(x%period+period)%period;y=(y%period+period)%period;
    std::uint32_t h=std::uint32_t(x)*0x8da6b343u^std::uint32_t(y)*0xd8163841u^0xcb1ab31fu;
    h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;return h^(h>>16);
}
// Seamless irregular rounded bubble cavities. Generated once, mipmapped on the
// GPU, never evaluated as expensive cellular noise for every rendered pixel.
inline std::array<float,2> foamCell(float u,float v,int period){
    const float x=u*period,y=v*period;
    const int ix=int(std::floor(x)),iy=int(std::floor(y));
    float first=100,cavity=0,identity=0;
    for(int j=-1;j<=1;++j)for(int i=-1;i<=1;++i){
        const auto h=foamHash(ix+i,iy+j,period);
        const float px=ix+i+.15f+.7f*float(h&65535)/65535;
        const float py=iy+j+.15f+.7f*float(h>>16)/65535;
        const float d=(px-x)*(px-x)+(py-y)*(py-y);
        if(d<first){first=d;identity=float(h&255)/255;}
        const float radius=.34f+.27f*float((h>>8)&255)/255;
        const float inside=std::clamp((radius-std::sqrt(d))/(radius*.55f),0.f,1.f);
        cavity=std::max(cavity,inside*inside*(3-2*inside));
    }
    return {cavity,identity};
}
inline std::vector<std::uint8_t> foamTexture(){
    std::vector<std::uint8_t> data(foamTextureSize*foamTextureSize*4);
    for(int y=0;y<foamTextureSize;++y)for(int x=0;x<foamTextureSize;++x){
        const float u=(x+.5f)/foamTextureSize,v=(y+.5f)/foamTextureSize;
        const auto coarse=foamCell(u,v,8),fine=foamCell(u,v,31);
        const int i=(y*foamTextureSize+x)*4;
        data[i]=std::uint8_t(coarse[0]*255);data[i+1]=std::uint8_t(fine[0]*255);
        data[i+2]=std::uint8_t(coarse[1]*255);data[i+3]=255;
    }
    return data;
}
}
