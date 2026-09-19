#include "take/DollyCamera.h"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace take {
namespace {

float valueAt(const DollyCameraKeyframe& keyframe,std::size_t index){
    if(index<3)return index==0?keyframe.position.x:index==1?keyframe.position.y:keyframe.position.z;
    if(index<6)return index==3?keyframe.rotationDegrees.x:index==4?keyframe.rotationDegrees.y:keyframe.rotationDegrees.z;
    return keyframe.fov;
}
void setValue(DollyCameraKeyframe& keyframe,std::size_t index,float value){
    if(index==0)keyframe.position.x=value;else if(index==1)keyframe.position.y=value;else if(index==2)keyframe.position.z=value;
    else if(index==3)keyframe.rotationDegrees.x=value;else if(index==4)keyframe.rotationDegrees.y=value;else if(index==5)keyframe.rotationDegrees.z=value;else keyframe.fov=value;
}
float linearChannel(const std::vector<DollyCameraKeyframe>& keyframes,std::size_t channel,float tick){
    if(keyframes.size()==1)return valueAt(keyframes.front(),channel);std::size_t lower{},upper{1};for(std::size_t i=0;i+1<keyframes.size();++i)if(tick>=keyframes[i].tick&&tick<=keyframes[i+1].tick){lower=i;upper=i+1;break;}
    const float alpha=(tick-keyframes[lower].tick)/static_cast<float>(keyframes[upper].tick-keyframes[lower].tick);
    const float v0=valueAt(keyframes[lower],channel);
    float v1=valueAt(keyframes[upper],channel);
    if(channel>=3&&channel<=5){
        float diff=v1-v0;
        while(diff>180.0f)diff-=360.0f;
        while(diff<-180.0f)diff+=360.0f;
        return v0+diff*alpha;
    }
    return v0+(v1-v0)*alpha;
}
float cubicChannel(const std::vector<DollyCameraKeyframe>& keyframes,std::size_t channel,float tick){
    constexpr std::size_t maximum=256;const auto count=keyframes.size();if(count<2)throw std::runtime_error("Not enough camera nodes");if(count>maximum)return valueAt(keyframes.back(),channel);
    std::array<float,maximum> ticks{},values{},second{},work{};
    for(std::size_t i=0;i<count;++i){ticks[i]=static_cast<float>(keyframes[i].tick);values[i]=valueAt(keyframes[i],channel);}
    if(channel>=3&&channel<=5){
        for(std::size_t i=1;i<count;++i){
            float diff=values[i]-values[i-1];
            while(diff>180.0f){values[i]-=360.0f;diff-=360.0f;}
            while(diff<-180.0f){values[i]+=360.0f;diff+=360.0f;}
        }
    }
    second[0]=-0.5f;work[0]=(3.0f/(ticks[1]-ticks[0]))*((values[1]-values[0])/(ticks[1]-ticks[0]));
    for(std::size_t i=1;i<=count-2;++i){const float sigma=(ticks[i]-ticks[i-1])/(ticks[i+1]-ticks[i-1]),p=sigma*second[i-1]+2.0f;second[i]=(sigma-1.0f)/p;work[i]=(values[i+1]-values[i])/(ticks[i+1]-ticks[i])-(values[i]-values[i-1])/(ticks[i]-ticks[i-1]);work[i]=(6.0f*work[i]/(ticks[i+1]-ticks[i-1])-sigma*work[i-1])/p;}
    constexpr float qn=0.5f;const float un=(3.0f/(ticks[count-1]-ticks[count-2]))*(0.0f-(values[count-1]-values[count-2])/(ticks[count-1]-ticks[count-2]));second[count-1]=(un-qn*work[count-2])/(qn*second[count-2]+1.0f);for(int i=static_cast<int>(count)-2;i>=0;--i)second[static_cast<std::size_t>(i)]=second[static_cast<std::size_t>(i)]*second[static_cast<std::size_t>(i+1)]+work[static_cast<std::size_t>(i)];
    std::size_t lower{},upper=count-1;while(upper-lower>1){const auto middle=(upper+lower)>>1;if(ticks[middle]>tick)upper=middle;else lower=middle;}const float span=ticks[upper]-ticks[lower],a=(ticks[upper]-tick)/span,b=(tick-ticks[lower])/span;return a*values[lower]+b*values[upper]+((a*a*a-a)*second[lower]+(b*b*b-b)*second[upper])*(span*span)/6.0f;
}

} // namespace

DollyCameraKeyframe interpolateDollyCamera(const std::vector<DollyCameraKeyframe>& keyframes,float tick){
    if(keyframes.empty())return {};if(tick<keyframes.front().tick)return keyframes.front();if(tick>keyframes.back().tick)return keyframes.back();DollyCameraKeyframe result;result.tick=static_cast<std::uint32_t>(std::max(0.0f,tick));for(std::size_t channel=0;channel<7;++channel)setValue(result,channel,keyframes.size()<4?linearChannel(keyframes,channel,tick):cubicChannel(keyframes,channel,tick));return result;
}

} // namespace take
