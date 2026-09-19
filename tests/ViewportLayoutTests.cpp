#include "app/ViewportLayout.h"
#include <iostream>
#include <limits>
int main(){
    int failures=0;const auto expect=[&](bool v){if(!v)++failures;};
    for(float w:{1.f,500.f,1920.f})for(float h:{1.f,700.f,1080.f})for(float aspect:{.5f,1.f,16.f/9,3.6f})for(float scale:{.25f,.5f,1.f,1.5f})for(bool lock:{false,true}){
        auto r=cadence::viewportLayout(w,h,lock,aspect,scale,false,3840,2160);
        expect(r.renderWidth>=1&&r.renderHeight>=1&&r.width<=w+.001f&&r.height<=h+.001f&&r.left>=0&&r.top>=0);
        if(lock)expect(std::abs(r.width/r.height-aspect)<.001f);
        auto ex=cadence::viewportLayout(w,h,lock,aspect,scale,true,3840,2160);expect(ex.renderWidth==3840&&ex.renderHeight==2160);
    }
    auto r=cadence::viewportLayout(1920,1200,true,16.f/9,.5f,false,1,1);expect(r.renderWidth==960&&r.renderHeight==540&&r.top==60);
    const float nan=std::numeric_limits<float>::quiet_NaN();r=cadence::viewportLayout(nan,-5,true,nan,nan,false,0,0);expect(r.renderWidth>=1&&r.renderHeight>=1&&std::isfinite(r.width));
    std::cout<<"Viewport/export layout failures: "<<failures<<'\n';return failures?1:0;
}
