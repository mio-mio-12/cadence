#pragma once
#include "scene/ImportedBodyRetarget.h"
#include <array>
#include <vector>

namespace gameplay::ragdoll {
struct DroppedWeapon;
struct Settings {
    bool enabled{};
    float movementMomentum{.65f},shotImpulse{180.f};
    float blendSeconds{.18f},gravity{1.f},damping{2.f},friction{.7f},momentum{1.f},radius{4.f},settleSeconds{8.f};
};
// Small position-based articulated solver, in world centimetres. Captured bone
// matrices keep their original scale; simulation never refits mesh proportions.
struct State {
    struct Point {scene::Vec3 position{},previous{},start{};int bone{-1},aim{-1};};
    struct Link {int a{},b{};float minimum{},maximum{};};
    std::vector<Point> points;
    std::vector<Link> links;
    std::vector<scene::Mat4> initial,history,previousHistory;
    std::vector<int> owner;
    Settings settings;
    bool active{},attempted{};
    double deathStamp{-1e30};
    float accumulator{},elapsed{},historyDelta{};
    int variant{},weaponDropTrigger{};
    std::shared_ptr<DroppedWeapon> weapon;
    static scene::Mat4 torsoFrame(scene::Vec3 hip,scene::Vec3 spine,scene::Vec3 left,scene::Vec3 right){
        auto z=scene::normalize(spine-hip),x=scene::normalize(left-right);
        auto y=scene::normalize(scene::cross(z,x));x=scene::normalize(scene::cross(y,z));
        auto m=scene::Mat4::identity();m.v[0]=x.x;m.v[1]=x.y;m.v[2]=x.z;m.v[4]=y.x;m.v[5]=y.y;m.v[6]=y.z;m.v[8]=z.x;m.v[9]=z.y;m.v[10]=z.z;return m;
    }
    static scene::Vec3 origin(const scene::Mat4& m){return {m.v[12],m.v[13],m.v[14]};}
    static scene::Quat arc(scene::Vec3 a,scene::Vec3 b){
        a=scene::normalize(a);b=scene::normalize(b);const float d=scene::dot(a,b);
        if(scene::length(a)<.5f||scene::length(b)<.5f)return {};
        if(d<-.9999f){auto axis=scene::cross(a,{1,0,0});if(scene::length(axis)<.01f)axis=scene::cross(a,{0,1,0});return scene::fromAxisAngle(axis,scene::kPi);}
        const auto c=scene::cross(a,b);return scene::normalize(scene::Quat{c.x,c.y,c.z,1.f+d});
    }
    bool begin(const scene::Skeleton& skeleton,const std::vector<scene::Mat4>& pose,const Settings& config,scene::Vec3 deathVelocity={},scene::Vec3 shotDirection={}){
        attempted=true;if(pose.size()!=skeleton.bones.size())return false;
        const auto b=scene::imported::bodyOrCodLayout(skeleton);
        const std::array<int,15> bones{b.pelvis,b.spine,b.head,b.shoulder[0],b.elbow[0],b.hand[0],b.shoulder[1],b.elbow[1],b.hand[1],b.thigh[0],b.knee[0],b.foot[0],b.thigh[1],b.knee[1],b.foot[1]};
        for(int i:bones)if(i<0||i>=int(pose.size()))return false;
        settings=config;initial=pose;points.clear();links.clear();
        for(int i:bones){const auto p=origin(pose[i]);auto v=scene::Vec3{};
            if(history.size()==pose.size()&&historyDelta>0)v=(p-origin(history[i]))/historyDelta;
            const float speed=scene::length(v);if(speed>600)v=v*(600.f/speed);
            v=v*settings.momentum+deathVelocity*settings.movementMomentum+
                scene::normalize(shotDirection)*settings.shotImpulse;
            points.push_back({p,p-v*(1.f/120.f),p,i,-1});
        }
        const auto link=[&](int a,int c,float lo=1.f,float hi=1.f){const float l=scene::length(points[a].start-points[c].start);if(l>.001f)links.push_back({a,c,l*lo,l*hi});};
        // Rigid pelvis/chest frame; articulated arms, legs and neck.
        const std::array<int,6> torso{0,1,3,6,9,12};
        for(int i=0;i<6;++i)for(int j=i+1;j<6;++j)link(torso[i],torso[j]);
        link(1,2);link(3,4);link(4,5);link(6,7);link(7,8);
        link(9,10);link(10,11);link(12,13);link(13,14);
        // Limit folding without freezing the final animated bend.
        link(3,5,.45f,1.15f);link(6,8,.45f,1.15f);link(9,11,.55f,1.1f);link(12,14,.55f,1.1f);link(0,2,.85f,1.1f);
        const std::array<int,15> aims{1,2,1,4,5,4,7,8,7,10,11,10,13,14,13};
        for(int i=0;i<15;++i)points[i].aim=aims[i];
        owner.assign(pose.size(),0);
        for(int i=0;i<int(pose.size());++i){int parent=i;for(size_t depth=0;depth<pose.size()&&parent>=0&&parent<int(pose.size());++depth){
            auto it=std::find(bones.begin(),bones.end(),parent);if(it!=bones.end()){owner[i]=int(it-bones.begin());break;}parent=skeleton.bones[parent].parent;
        }}
        active=true;accumulator=elapsed=0;return true;
    }
    // Contact callback returns a corrected sphere centre. Fixed steps and a
    // bounded lifetime keep dead actors from growing unbounded physics costs.
    template<class Contact> void advance(float dt,Contact contact){
        if(!active||(settings.settleSeconds>0&&elapsed>=settings.settleSeconds))return;
        accumulator=std::min(accumulator+std::max(0.f,dt),.1f);constexpr float h=1.f/120.f;
        while(accumulator>=h&&(settings.settleSeconds<=0||elapsed<settings.settleSeconds)){accumulator-=h;elapsed+=h;
            std::array<scene::Vec3,15> old;
            for(int i=0;i<15;++i){auto& p=points[i];old[i]=p.position;auto velocity=(p.position-p.previous)*std::exp(std::clamp(-settings.damping*h,-40.f,10.f));const float speed=scene::length(velocity);if(speed>100)velocity=velocity*(100.f/speed);p.previous=p.position;p.position+=velocity+scene::Vec3{0,0,std::clamp(-981.f*settings.gravity*h*h,-100.f,100.f)};}
            for(int iteration=0;iteration<8;++iteration){
                for(const auto& l:links){auto& a=points[l.a].position;auto& b=points[l.b].position;const auto d=b-a;const float n=scene::length(d);if(n>.0001f){const auto correction=d*((n-std::clamp(n,l.minimum,l.maximum))*.5f/n);a+=correction;b-=correction;}}
            }
            for(int i=0;i<15;++i)points[i].position=contact(old[i],points[i].position,std::max(0.f,settings.radius));
            for(int i=0;i<15;++i){auto& p=points[i];const auto resolved=contact(p.position,p.position+scene::Vec3{0,0,-.1f},std::max(0.f,settings.radius));
                if(scene::length(resolved-(p.position+scene::Vec3{0,0,-.1f}))>.01f)p.previous=scene::lerp(p.previous,p.position,settings.friction);
            }
            const auto finite=[](scene::Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);};
            if(std::any_of(points.begin(),points.end(),[&](const auto& p){return !finite(p.position)||!finite(p.previous);})){for(int i=0;i<15;++i)points[i].position=points[i].previous=old[i];settings.settleSeconds=elapsed;break;}
        }
    }
    void apply(std::vector<scene::Mat4>& pose)const{
        if(!active||pose.size()!=initial.size())return;
        const float t=std::clamp(elapsed/std::max(.001f,settings.blendSeconds),0.f,1.f),weight=t*t*(3-2*t);
        std::array<scene::Mat4,15> delta;
        const auto before=torsoFrame(points[0].start,points[1].start,points[3].start,points[6].start);
        const auto after=torsoFrame(points[0].position,points[1].position,points[3].position,points[6].position);
        scene::Vec3 unused,scale;scene::Quat torsoRotation;scene::decomposeAffine(after*scene::inverseAffine(before),unused,torsoRotation,scale);
        const auto torso=scene::translation(scene::lerp(points[0].start,points[0].position,weight))*scene::rotation(scene::slerp({},torsoRotation,weight))*scene::translation(-points[0].start);
        for(int i=0;i<15;++i){const auto& p=points[i];const auto& aim=points[p.aim];
            const auto q=scene::slerp(scene::Quat{},arc(aim.start-p.start,aim.position-p.position),weight);
            delta[i]=scene::translation(scene::lerp(p.start,p.position,weight))*scene::rotation(q)*scene::translation(-p.start);
        }
        // The skinned pelvis/spine/chest must share one rigid transform. Using
        // independent look-at rotations at each torso joint tears their seam.
        delta[0]=delta[1]=torso;
        for(size_t i=0;i<pose.size();++i)pose[i]=delta[owner[i]]*initial[i];
    }
};
struct Corpse {
    State state;
    std::vector<scene::Mat4> pose;
    std::uint32_t id{};
    float fade{-1.f},opacity{1.f};
};
struct Bodies {
    bool keep{};int maximum{16};float fadeSeconds{1.f};
    std::uint32_t nextId{1};std::vector<Corpse> bodies;
    void retain(State&& state,const std::vector<scene::Mat4>& pose){
        if(!keep||!state.active||maximum<=0)return;
        bodies.push_back({std::move(state),pose,nextId++});
    }
    template<class Contact> void advance(float dt,Contact contact){
        int count=0;for(const auto& b:bodies)if(b.fade<0)++count;
        for(auto& b:bodies){if(b.fade<0&&(!keep||count>std::max(0,maximum))){b.fade=0;--count;}
            b.state.advance(dt,contact);b.state.apply(b.pose);
            if(b.fade>=0){b.fade+=std::max(0.f,dt);b.opacity=fadeSeconds>0?std::clamp(1.f-b.fade/fadeSeconds,0.f,1.f):0;}
        }
        std::erase_if(bodies,[](const auto& b){return b.opacity<=0;});
        // Bound transient fading bodies as well as retained bodies.
        if(bodies.size()>160)bodies.erase(bodies.begin(),bodies.end()-160);
    }
};
}
