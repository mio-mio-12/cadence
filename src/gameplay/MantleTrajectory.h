#pragma once
#include "gameplay/MovementTrace.h"
#include "gameplay/IwPresentation.h"

namespace gameplay::mantle {
// Presentation must not inherit the very short momentum-preserving traversal.
struct Presentation {
    double start{-1};
    float duration{};
    void begin(double now,float traversal,float heightIw,float speed) {
        start=now;
        duration=std::max(traversal,(.4f+.25f*std::clamp(heightIw/72.f,0.f,1.f))/std::max(.1f,speed));
    }
    float phase(double now) const {return start<0?1.f:std::clamp(static_cast<float>(now-start)/std::max(.001f,duration),0.f,1.f);}
    bool active(double now) const {return start>=0&&phase(now)<1.f;}
    void cancel(){start=-1;}
};
struct Sample { scene::Vec3 position{},velocity{}; };
// A short, bounded presentation spring removes the camera's abrupt change in
// vertical acceleration without changing traversal, collision or momentum.
struct CameraSmoothing {
    float z{},velocity{},remaining{};
    void begin(float position,float speed){z=position;velocity=speed;remaining=.2f;}
    float update(float target,float dt,bool mantling){
        if(mantling)remaining=.2f;else remaining=std::max(0.f,remaining-dt);
        if(dt<=0)return z;
        constexpr float omega=32.f;
        const float offset=z-target,term=velocity+omega*offset,e=std::exp(-omega*dt);
        z=target+(offset+term*dt)*e;velocity=(velocity-omega*term*dt)*e;
        z=std::clamp(z,target-24.f,target+24.f);
        if(!mantling&&remaining<=0&&(std::abs(z-target)>.05f||std::abs(velocity)>1.f))remaining=.01f;
        return z;
    }
};
struct Trajectory {
    scene::Vec3 start{},target{},entry{};
    float weight{},duration{},riseTime{};
    float playbackSpeed{1.0f};
    bool liftFirst{};
    float clearanceZ{-std::numeric_limits<float>::max()};
    float entryVertical{};
    static float smooth(float t){t=std::clamp(t,0.0f,1.0f);return t*t*(3-2*t);}
    static float derivative(float t){return t>0&&t<1?6*t*(1-t):0;}
    Sample sample(float time) const {
        if(liftFirst){
            const float t=std::clamp(time/duration,0.f,1.f);
            const float apex=std::max(target.z,clearanceZ);
            const float vertical=std::clamp(t/.45f,0.f,1.f),horizontal=std::clamp((t-.45f)/.4f,0.f,1.f),settle=std::clamp((t-.85f)/.15f,0.f,1.f);
            auto p=scene::lerp(start,target,smooth(horizontal));p.z=start.z+(apex-start.z)*smooth(vertical)+(target.z-apex)*smooth(settle);
            auto v=(target-start)*(derivative(horizontal)/(.4f*duration));v.z=(apex-start.z)*derivative(vertical)/(.45f*duration)+(target.z-apex)*derivative(settle)/(.15f*duration);
            return {p,v};
        }
        // Reparameterize time, preserving the collision path and its endpoint.
        time*=playbackSpeed;
        const float duration=this->duration*playbackSpeed,riseTime=this->riseTime*playbackSpeed;
        const float t=std::clamp(time/duration,0.0f,1.0f);
        // Slow climb: lift first, then controlled placement. Fast vault:
        // integrate the incoming vector, without attraction or end easing.
        const float h=std::clamp((t-0.35f)/0.65f,0.0f,1.0f);
        const scene::Vec3 slow=scene::lerp(start,target,smooth(h))+entry*(time*(1-t)*(1-t));
        const scene::Vec3 slowV=(target-start)*(derivative(h)/(duration*0.65f))+entry*(1-4*t+3*t*t);
        Sample out;
        out.position=scene::lerp(slow,start+entry*time,weight);
        out.velocity=scene::lerp(slowV,entry,weight);
        const float u=std::clamp(time/riseTime,0.0f,1.0f);
        const float fallDuration=duration-riseTime;
        const float fallTime=std::max(0.0f,time-riseTime);
        const float g=gameplay::iw::kGravity*weight;
        const float lift=target.z-start.z+gameplay::iw::worldUnits(2.0f)*weight+0.5f*g*fallDuration*fallDuration;
        out.position.z=start.z+lift*smooth(u)-0.5f*g*fallTime*fallTime;
        out.velocity.z=lift*derivative(u)/riseTime-g*fallTime;
        // Hermite entry tangent: an airborne mantle need not cancel its jump.
        out.position.z+=entryVertical*riseTime*u*(1-u)*(1-u);
        if(u<1)out.velocity.z+=entryVertical*(1-4*u+3*u*u);
        out.velocity=out.velocity*playbackSpeed;
        return out;
    }
};
inline Trajectory plan(scene::Vec3 start,scene::Vec3 target,scene::Vec3 velocity,scene::Vec3 facing,float speedMultiplier=1.0f) {
    velocity.z=0;facing.z=0;facing=scene::normalize(facing);
    const float toward=std::max(0.0f,scene::dot(velocity,facing));
    const float speed=std::hypot(velocity.x,velocity.y);
    const float blend=Trajectory::smooth((toward-gameplay::iw::worldUnits(60))/gameplay::iw::worldUnits(150));
    const float height=std::max(0.0f,target.z-start.z);
    const float slowTime=0.4f+0.25f*std::clamp(height/gameplay::iw::worldUnits(72),0.0f,1.0f);
    const float distance=std::hypot(target.x-start.x,target.y-start.y);
    const float fastTime=std::clamp(distance/std::max(speed,1.0f),0.12f,0.65f);
    const float duration=slowTime+(fastTime-slowTime)*blend;
    const float rate=std::clamp(speedMultiplier,0.25f,3.0f);
    return {start,target,velocity,blend,duration/rate,duration*(0.65f-0.10f*blend)/rate,rate};
}
template<class Sweep> bool clear(const Trajectory& path,Sweep&& sweep) {
    auto previous=path.start;
    const int steps=std::max(1,static_cast<int>(std::ceil(path.duration*path.playbackSpeed/0.004f)));
    for(int i=1;i<=steps;++i) {
        auto next=path.sample(path.duration*static_cast<float>(i)/steps).position;
        const auto hit=sweep(previous,next);
        if(hit.startSolid||hit.fraction<0.9999f)return false;
        previous=next;
    }
    return true;
}

// Upgrade an already supported climb, retaining the safe lift-first fallback
// when there is genuinely no room to move forward before clearing the lip.
template<class Sweep> Trajectory withMomentum(const Trajectory& fallback,scene::Vec3 velocity,
    scene::Vec3 facing,float rate,Sweep&& sweep){
    rate=std::clamp(rate,.25f,3.f);
    auto path=plan(fallback.start,fallback.target,velocity,facing,rate);
    if(path.weight<.5f)return fallback;
    for(float fraction:{.55f,.4f,.28f,.18f}){
        path.riseTime=std::max(.035f/rate,path.duration*fraction);
        path.riseTime=std::min(path.riseTime,path.duration*.8f);
        path.entryVertical=velocity.z/std::max(.25f,rate);
        if(clear(path,sweep))return path;
    }
    return fallback;
}

struct ApproachMomentum {
    scene::Vec3 velocity{};float remaining{};
    void age(float dt,bool enabled){remaining=enabled?std::max(0.f,remaining-dt):0.f;}
    void contact(scene::Vec3 before,scene::Vec3 after,bool intent){
        const float speed=std::hypot(before.x,before.y);
        if(intent&&speed>gameplay::iw::worldUnits(100)&&std::hypot(after.x,after.y)<speed*.7f){velocity=before;remaining=.08f;}
    }
    scene::Vec3 entry(scene::Vec3 current,scene::Vec3 facing)const{
        if(remaining<=0||scene::dot(velocity,facing)<=0)return current;
        const float previous=std::hypot(velocity.x,velocity.y),now=std::hypot(current.x,current.y);
        if(previous<=now||scene::dot(velocity,facing)<previous*.7f)return current;
        return {velocity.x,velocity.y,current.z};
    }
};
} // namespace gameplay::mantle
