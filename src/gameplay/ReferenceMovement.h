#pragma once
#include "gameplay/MovementTrace.h"

// Native-unit simulation, with a trace callback at the engine boundary.
// Independently expressed from the references pinned in docs/REFERENCE_MOVEMENT_V244.md.
// No changes to the legacy controllers. See the document for parity limits.
namespace gameplay::reference {
using scene::Vec3;
enum class Rules { HalfLife, Cod };
inline bool enabled(int mode) { return mode == 4 || mode == 5; }
inline double interval(int mode) { return mode == 4 ? .010 : .008; }
struct State {
    Vec3 position{}, velocity{};
    bool grounded{}, jumpHeld{};
    int clockMs{}, lastJumpMs{-1000}, landingMs{};
    float jumpOrigin{};
    bool jumped{};
};
struct Command {
    float forward{}, side{}, yaw{};
    bool jump{}, sprint{}, ads{};
    // Cadence input convenience: repeat a held press only once grounded. The
    // native release latch remains available for reference-equation checks.
    bool autoJump{true};
    int stance{}; // stand, crouch, prone
    float weaponScale{1};
};
inline Vec3 direction(const Command& cmd) {
    const float c=std::cos(cmd.yaw),s=std::sin(cmd.yaw);
    return {cmd.forward*c+cmd.side*s,cmd.forward*s-cmd.side*c,0};
}
inline Vec3 wish(const Command& cmd, Rules rules, bool ground) {
    auto v=direction(cmd);const float magnitude=scene::length(v);
    if(magnitude==0)return {};
    if(rules==Rules::HalfLife)
        return v*(320.f*std::min(1.f,magnitude)/magnitude*(cmd.stance? .333f:1.f));
    // Command scaling changes magnitude, NOT the direction of diagonal input.
    const float maximum=ground?std::max(std::abs(cmd.forward)*(cmd.forward<0?.7f:1.f),std::abs(cmd.side)*.8f)
                              :std::max(std::abs(cmd.forward),std::abs(cmd.side));
    float scale=190.f*maximum/magnitude;
    if(ground)scale*=cmd.weaponScale*(cmd.ads?.4f:1.f)*(cmd.sprint?1.5f:1.f)*(cmd.stance==2?.15f:cmd.stance==1?.65f:1.f);
    return v*scale;
}
inline void accelerate(Vec3& velocity,Vec3 wishVelocity,float dt,Rules rules,bool ground,int stance=0) {
    const float speed=scene::length(wishVelocity);if(speed==0)return;
    const Vec3 dir=wishVelocity/speed;
    const float add=(rules==Rules::HalfLife&&!ground?std::min(speed,30.f):speed)-scene::dot(velocity,dir);
    if(add<=0)return;
    const float acceleration=rules==Rules::HalfLife?10.f:ground?(stance==2?19.f:stance==1?12.f:9.f):1.f;
    velocity+=dir*std::min(add,acceleration*dt*(rules==Rules::Cod?std::max(speed,100.f):speed));
}
inline Vec3 clip(Vec3 v,Vec3 n,Rules rules) {
    const float d=scene::dot(v,n);
    const float backoff=rules==Rules::Cod?d-std::abs(d)*.001f:d;
    v=v-n*backoff;
    if(rules==Rules::HalfLife){if(std::abs(v.x)<.1f)v.x=0;if(std::abs(v.y)<.1f)v.y=0;if(std::abs(v.z)<.1f)v.z=0;}
    return v;
}
// IW projects a horizontal vector onto the ground without losing its length.
inline Vec3 projectGround(Vec3 v,Vec3 n) {
    if(n.z<.001f)return v;
    const float z=-(v.x*n.x+v.y*n.y)/n.z;
    const float length=scene::length(v),out=std::sqrt(v.x*v.x+v.y*v.y+z*z);
    if(out>0 && (out>length || z<0 || v.z>0))return Vec3{v.x,v.y,z}*(length/out);
    return v;
}
template<class Trace> void slide(State& state,float dt,Rules rules,Trace&& trace) {
    Vec3 planes[5]{};int count=0;float remaining=dt,total=0;
    const Vec3 primal=state.velocity;Vec3 segment=state.velocity;
    for(int bump=0;bump<4;++bump){
        if(scene::length(state.velocity)==0)break;
        const auto hit=trace(state.position,state.position+state.velocity*remaining);
        total+=hit.fraction;
        if(hit.startSolid){state.velocity={};return;}
        if(hit.fraction>0){state.position=hit.end;segment=state.velocity;count=0;}
        if(hit.fraction>=1)break;
        remaining*=1-hit.fraction;
        if(count>=5){state.velocity={};break;}
        planes[count++]=hit.normal;
        bool found=false;
        for(int i=0;i<count;++i){
            auto candidate=clip(segment,planes[i],rules);bool valid=true;
            for(int j=0;j<count;++j)if(j!=i&&scene::dot(candidate,planes[j])<0){valid=false;break;}
            state.velocity=candidate;if(valid){found=true;break;}
        }
        if(!found){
            if(count!=2){state.velocity={};break;}
            auto crease=scene::cross(planes[0],planes[1]);
            // GoldSrc intentionally does not normalize this crease.
            if(rules==Rules::Cod)crease=scene::normalize(crease);
            state.velocity=crease*scene::dot(state.velocity,crease);
        }
        // GoldSrc's airborne first contact updates the segment velocity even
        // when the next trace makes no progress. Later planes clip that result,
        // not the velocity from before the first collision (PM_FlyMove).
        if(rules==Rules::HalfLife&&!state.grounded&&count==1)segment=state.velocity;
        if(scene::dot(state.velocity,primal)<=0){state.velocity={};break;}
    }
    if(total==0)state.velocity={};
}
template<class Trace> void walk(State& state,float dt,float step,Rules rules,Trace&& trace) {
    const State start=state;
    const auto direct=trace(state.position,state.position+state.velocity*dt);
    if(!direct.startSolid&&direct.fraction==1){state.position=direct.end;return;}
    slide(state,dt,rules,trace);const State low=state;
    State high=start;const auto up=trace(start.position,start.position+Vec3{0,0,step});
    if(up.startSolid)return;
    high.position=up.end;slide(high,dt,rules,trace);
    const auto down=trace(high.position,high.position-Vec3{0,0,step});
    if(down.startSolid||down.fraction==1||down.normal.z<.7f)return;
    high.position=down.end;
    const auto distance=[&](Vec3 v){v=v-start.position;return v.x*v.x+v.y*v.y;};
    if(distance(high.position)>=distance(low.position)){high.velocity.z=low.velocity.z;state=high;}
}
template<class Trace> bool codSlide(State& state,float dt,Vec3 groundNormal,bool groundPlane,bool gravity,Trace&& trace) {
    Vec3 endVelocity=state.velocity;
    if(gravity){endVelocity.z-=800*dt;state.velocity.z=(state.velocity.z+endVelocity.z)*.5f;
        if(groundPlane)state.velocity=clip(state.velocity,groundNormal,Rules::Cod);}
    std::array<Vec3,8> planes{};int count=0;bool bumped=false;
    if(groundPlane)planes[count++]=groundNormal;
    planes[count++]=scene::normalize(state.velocity);
    float remaining=dt;
    for(int bump=0;bump<4&&remaining>0;++bump){
        const auto hit=trace(state.position,state.position+state.velocity*remaining);
        if(hit.startSolid){state.velocity.z=0;return true;}
        if(hit.fraction>0)state.position=hit.end;
        if(hit.fraction==1)break;
        bumped=true;remaining*=1-hit.fraction;
        if(count==8){state.velocity={};return true;}
        bool duplicate=false;
        for(int i=0;i<count;++i)if(scene::dot(hit.normal,planes[i])>.999f){duplicate=true;break;}
        if(duplicate){state.velocity=clip(state.velocity,hit.normal,Rules::Cod)+hit.normal;continue;}
        planes[count++]=hit.normal;
        std::array<int,8> order{};for(int i=0;i<count;++i)order[i]=i;
        std::stable_sort(order.begin(),order.begin()+count,[&](int a,int b){return scene::dot(state.velocity,planes[a])<scene::dot(state.velocity,planes[b]);});
        const auto first=planes[order[0]];
        if(scene::dot(state.velocity,first)>=.1f)continue;
        auto candidate=clip(state.velocity,first,Rules::Cod),end=clip(endVelocity,first,Rules::Cod);
        for(int j=1;j<count;++j){
            const auto plane=planes[order[j]];
            if(scene::dot(candidate,plane)>=.1f)continue;
            candidate=clip(candidate,plane,Rules::Cod);end=clip(end,plane,Rules::Cod);
            if(scene::dot(candidate,first)>=0)continue;
            const auto crease=scene::normalize(scene::cross(first,plane));
            candidate=crease*scene::dot(state.velocity,crease);end=crease*scene::dot(endVelocity,crease);
            for(int k=1;k<count;++k)if(k!=j&&scene::dot(candidate,planes[order[k]])<.1f){state.velocity={};return true;}
        }
        state.velocity=candidate;endVelocity=end;
    }
    if(gravity)state.velocity=endVelocity;
    return bumped;
}
template<class Trace> void codStep(State& state,float dt,Vec3 normal,bool groundPlane,int stance,Trace&& trace){
    const State start=state;const bool airborne=!state.grounded;
    const bool bumped=codSlide(state,dt,normal,groundPlane,airborne,trace);const State low=state;
    float step=stance==2?10.f:18.f;
    if(airborne){if(!bumped||!state.jumped)return;step=std::min(step,state.jumpOrigin+39-start.position.z);if(step<1)return;}
    float height=0;
    if(bumped||(groundPlane&&normal.z<.9f)){
        const auto up=trace(start.position,start.position+Vec3{0,0,step+1});
        const float amount=(step+1)*up.fraction-1;
        if(!up.startSolid&&amount>=1){height=amount;state=start;state.position.z+=height;codSlide(state,dt,normal,groundPlane,airborne,trace);}
    }
    if(groundPlane||height!=0){
        const auto down=trace(state.position,state.position-Vec3{0,0,height+(groundPlane?9.f:0.f)});
        if(down.fraction==1)state.position.z-=height;
        else if(!down.startSolid&&down.normal.z>=.3f){state.position=down.end;state.velocity=projectGround(state.velocity,down.normal);}
        else {state=low;return;}
    }
    const auto progress=[&](Vec3 p){p=p-start.position;return start.velocity.x*p.x+start.velocity.y*p.y;};
    if(progress(state.position)<=progress(low.position)+.001f){
        state=low;
        if(groundPlane){const auto down=trace(state.position,state.position-Vec3{0,0,9});if(!down.startSolid&&down.fraction<1){state.position=down.end;state.velocity=clip(state.velocity,down.normal,Rules::Cod);}}
    }
}
inline float snap(float v){
    const float a=std::abs(v),base=std::floor(a),fraction=a-base;
    return std::copysign(base+(fraction>.5f||(fraction==.5f&&std::fmod(base,2.f)!=0)?1.f:0.f),v);
}
inline void boundHalfLifeVelocity(Vec3& velocity){
    velocity.x=std::clamp(velocity.x,-2000.f,2000.f);
    velocity.y=std::clamp(velocity.y,-2000.f,2000.f);
    velocity.z=std::clamp(velocity.z,-2000.f,2000.f);
}
template<class Trace> auto ground(State& state,Rules rules,Trace&& trace) {
    const float depth=rules==Rules::HalfLife?2.f:.25f;
    const auto hit=trace(state.position,state.position-Vec3{0,0,depth});
    state.grounded=!hit.startSolid&&hit.fraction<1&&hit.normal.z>=.7f;
    if(rules==Rules::HalfLife){if(state.velocity.z>180)state.grounded=false;}
    else if(state.velocity.z>0&&scene::dot(state.velocity,hit.normal)>10)state.grounded=false;
    if(state.grounded)state.position=hit.end;
    return hit;
}
template<class Trace> void tick(State& state,const Command& cmd,int msec,Rules rules,Trace&& trace) {
    if(msec<=0||msec>100)return;
    const auto originalPosition=state.position;
    const float dt=msec*.001f;state.clockMs+=msec;state.landingMs=std::max(0,state.landingMs-msec);
    const auto floor=ground(state,rules,trace);
    const bool wasGrounded=state.grounded;
    if(rules==Rules::HalfLife){
        state.velocity.z-=400*dt;
        // PM_AddCorrectGravity bounds velocity before movement/jump handling.
        boundHalfLifeVelocity(state.velocity);
    }
    if(cmd.jump&&(cmd.autoJump||!state.jumpHeld)&&state.grounded&&(rules==Rules::HalfLife||(cmd.stance==0&&state.clockMs-state.lastJumpMs>=500))){
        if(rules==Rules::HalfLife){
            const float speed=scene::length(state.velocity);
            if(speed>320.f*1.7f)state.velocity=state.velocity*(320.f*1.7f*.65f/speed);
            state.velocity.z=std::sqrt(2.f*800*45)-400*dt;
        }else{
            state.velocity.z=std::sqrt(2.f*800*39/(state.landingMs>0?1.f+state.landingMs*1.5f/1700.f:1.f));
            state.lastJumpMs=state.clockMs;state.jumpOrigin=state.position.z;state.jumped=true;state.landingMs=0;
        }
        state.grounded=false;
    }
    state.jumpHeld=cmd.jump;
    if(rules==Rules::Cod&&scene::length(state.velocity)<1)state.velocity={};
    if(state.grounded){
        if(rules==Rules::HalfLife)state.velocity.z=0;
        const float speed=scene::length(state.velocity);
        float friction=rules==Rules::HalfLife?4.f:5.5f;
        if(rules==Rules::HalfLife&&speed>=.1f){
            const Vec3 ahead=state.position+Vec3{state.velocity.x,state.velocity.y,0}*(16/speed);
            if(trace(ahead,ahead-Vec3{0,0,34}).fraction==1)friction*=2;
        }
        if(rules==Rules::Cod&&state.landingMs>0)friction*=state.landingMs>1700?2.5f:1.f+state.landingMs*1.5f/1700.f;
        if(speed>0)state.velocity=state.velocity*(std::max(0.f,speed-std::max(speed,100.f)*friction*dt)/speed);
    }
    auto wishVelocity=wish(cmd,rules,state.grounded);
    if(rules==Rules::Cod&&state.grounded)wishVelocity=projectGround(wishVelocity,floor.normal);
    accelerate(state.velocity,wishVelocity,dt,rules,state.grounded,cmd.stance);
    if(rules==Rules::Cod){
        if(state.grounded)state.velocity=projectGround(state.velocity,floor.normal);
        codStep(state,dt,floor.normal,!floor.startSolid&&floor.fraction<1&&scene::dot(state.velocity,floor.normal)<=10,cmd.stance,trace);
    }else if(state.grounded){
        state.velocity.z=0;
        if(scene::length(state.velocity)<1)state.velocity={};
        walk(state,dt,18.f,rules,trace);
    }else{
        slide(state,dt,rules,trace);
    }
    ground(state,rules,trace);
    // The reference bounds collision output before the final half gravity,
    // then bounds it again afterwards. End-only clamping changes displacement.
    if(rules==Rules::HalfLife)boundHalfLifeVelocity(state.velocity);
    if(!state.grounded&&rules==Rules::HalfLife)state.velocity.z-=400*dt;
    else if(state.grounded){
        if(rules==Rules::HalfLife)state.velocity.z=0;
        if(rules==Rules::Cod&&!wasGrounded&&state.jumped){
            const bool lower=state.position.z<state.jumpOrigin;
            state.landingMs=lower?1800:1200;state.velocity=state.velocity*(lower?.65f:.5f);state.jumped=false;
        }
    }
    if(rules==Rules::HalfLife)boundHalfLifeVelocity(state.velocity);
    else {
        const auto actual=(state.position-originalPosition)/dt;
        if(scene::dot(actual,actual)<scene::dot(state.velocity,state.velocity)*.25f)state.velocity=actual;
        state.velocity={snap(state.velocity.x),snap(state.velocity.y),snap(state.velocity.z)};
    }
}
} // namespace gameplay::reference
