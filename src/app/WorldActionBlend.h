#pragma once
#include "scene/PoseEvaluationScratch.h"
#include "scene/WorldTorsoMask.h"
#include <optional>

namespace cadence {
// Local, channel-aware presentation only. Locomotion keeps advancing underneath
// an interrupted/finished action; roots and channels it never owned stay live.
struct WorldActionBlend {
    using Channels=scene::pose_detail::Channels;
    std::vector<scene::Transform> last,from,layer;
    std::vector<Channels> previousMask,fadeMask,mask;
    std::vector<bool> upperBody;
    bool torsoOnly{};
    std::optional<std::size_t> clip;
    const scene::Bone* bones{};
    const scene::Animation* animations{};
    float previousFrame{},elapsed{1.f};
    static Channels unite(Channels a,Channels b){return {a.tx||b.tx,a.ty||b.ty,a.tz||b.tz,a.rotation||b.rotation,a.sx||b.sx,a.sy||b.sy,a.sz||b.sz};}
    static void blend(scene::Transform& target,const scene::Transform& source,Channels c,float t){
        if(c.tx)target.position.x=source.position.x+(target.position.x-source.position.x)*t;
        if(c.ty)target.position.y=source.position.y+(target.position.y-source.position.y)*t;
        if(c.tz)target.position.z=source.position.z+(target.position.z-source.position.z)*t;
        if(c.rotation)target.rotation=scene::slerp(source.rotation,target.rotation,t);
        if(c.sx)target.scale.x=source.scale.x+(target.scale.x-source.scale.x)*t;
        if(c.sy)target.scale.y=source.scale.y+(target.scale.y-source.scale.y)*t;
        if(c.sz)target.scale.z=source.scale.z+(target.scale.z-source.scale.z)*t;
    }
    void apply(const scene::CastScene& actor,std::optional<std::size_t> next,float frame,float dt,float duration,std::vector<scene::Transform>& base,bool onlyTorso=false){
        if(base.size()!=actor.skeleton.bones.size())return;
        if(bones!=actor.skeleton.bones.data()||animations!=actor.animations.data()||last.size()!=base.size()||previousMask.size()!=base.size()||torsoOnly!=onlyTorso){
            *this={};bones=actor.skeleton.bones.data();animations=actor.animations.data();last=base;previousMask.resize(base.size());
            torsoOnly=onlyTorso;if(torsoOnly)upperBody=scene::worldTorsoMask(actor.skeleton);
        }
        if(next&&*next>=actor.animations.size())next.reset();
        mask.assign(base.size(),{});
        if(next){
            scene::pose_detail::markChannels(actor.animations[*next],actor.skeleton,mask);
            actor.sampleLocalPoseInto(*next,frame,layer);
            for(std::size_t i=0;i<base.size();++i){
                if(actor.skeleton.bones[i].parent<0||(torsoOnly&&!upperBody[i])){mask[i]={};continue;}
                blend(base[i],layer[i],mask[i],0.f);
            }
        }
        const bool changed=next!=clip||(next&&frame+.01f<previousFrame);
        if(changed){
            from=last;fadeMask.resize(base.size());
            for(std::size_t i=0;i<base.size();++i)fadeMask[i]=unite(previousMask[i],mask[i]);
            clip=next;elapsed=0;
        }else elapsed+=std::max(0.f,dt);
        const bool fading=from.size()==base.size()&&fadeMask.size()==base.size();
        const float t=fading&&std::isfinite(duration)&&duration>0?std::clamp(elapsed/duration,0.f,1.f):1.f,alpha=t*t*(3.f-2.f*t);
        if(t<1&&from.size()==base.size())for(std::size_t i=0;i<base.size();++i)blend(base[i],from[i],fadeMask[i],alpha);
        last=base;previousMask=t<1?fadeMask:mask;previousFrame=frame;
    }
};
}
