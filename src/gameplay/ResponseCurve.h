#pragma once
#include "scene/Math3D.h"
#include <istream>
#include <ostream>

namespace gameplay::view {
// General normalized curve. Endpoint values are editable (unlike recoil pulses).
struct ResponseCurve {
    float smoothness{1.f};
    int count{2};
    std::array<scene::Vec2,16> points{{{0,1},{1,1}}};
    static ResponseCurve linear(float start,float end){ResponseCurve c;c.smoothness=0;c.points[0].y=start;c.points[1].y=end;c.sanitize();return c;}
    void sanitize(){
        count=std::clamp(count,2,16);smoothness=std::isfinite(smoothness)?std::clamp(smoothness,0.f,1.f):1.f;
        for(int i=0;i<count;++i){auto& p=points[i];p.y=std::isfinite(p.y)?std::clamp(p.y,-1.f,3.f):0.f;
            if(i==0)p.x=0;else if(i==count-1)p.x=1;else p.x=std::clamp(std::isfinite(p.x)?p.x:float(i)/(count-1),points[i-1].x+.001f,1.f-(count-1-i)*.001f);}
    }
    float sample(float t)const{
        if(!std::isfinite(t)||t<=0)return points[0].y;
        if(t>=1)return points[count-1].y;
        for(int i=1;i<count;++i)if(t<=points[i].x){float u=std::clamp((t-points[i-1].x)/std::max(.001f,points[i].x-points[i-1].x),0.f,1.f);u+=(u*u*(3-2*u)-u)*smoothness;return points[i-1].y+(points[i].y-points[i-1].y)*u;}
        return points[count-1].y;
    }
};
inline std::ostream& operator<<(std::ostream& o,const ResponseCurve& c){o<<c.smoothness<<' '<<c.count;for(int i=0;i<c.count;++i)o<<' '<<c.points[i].x<<' '<<c.points[i].y;return o;}
inline std::istream& operator>>(std::istream& in,ResponseCurve& c){ResponseCurve v;in>>v.smoothness>>v.count;if(v.count<2||v.count>16){in.setstate(std::ios::failbit);return in;}for(int i=0;i<v.count;++i)in>>v.points[i].x>>v.points[i].y;if(in){v.sanitize();c=v;}return in;}
}
