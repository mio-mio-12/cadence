#pragma once
#include "scene/CastScene.h"
#include <span>
#include <memory>

namespace scene {
std::string canonicalName(std::string value);
namespace pose_detail {
struct Channels { bool tx{},ty{},tz{},rotation{},sx{},sy{},sz{}; };
struct TrackIdentity {
    std::size_t bone{};TrackProperty property{};bool owns{};
    bool matches(const Track& t)const{return bone==t.boneIndex&&property==t.property&&owns==t.ownsLayer;}
};
// Tracks are public and can be remapped in place. Exact metadata validation
// deliberately avoids pointer-only/revision-only invalidation or hash collisions.
// Curves, frames and modes do not affect channel ownership.
struct ChannelCache {
    const Animation* animation{};
    std::vector<TrackIdentity> tracks;
    std::vector<Channels> channels;
    void get(const Animation& a,std::size_t bones,std::vector<Channels>& result){
        bool same=animation==&a&&channels.size()==bones&&tracks.size()==a.tracks.size();
        if(same)for(std::size_t i=0;i<tracks.size();++i)if(!tracks[i].matches(a.tracks[i])){same=false;break;}
        if(!same){
            animation=&a;tracks.clear();tracks.reserve(a.tracks.size());channels.assign(bones,{});
            for(const auto& t:a.tracks){
                tracks.push_back({t.boneIndex,t.property,t.ownsLayer});
                if(!t.ownsLayer||t.boneIndex>=bones)continue;
                auto& c=channels[t.boneIndex];
                switch(t.property){
                case TrackProperty::TranslationX:c.tx=true;break;case TrackProperty::TranslationY:c.ty=true;break;
                case TrackProperty::TranslationZ:c.tz=true;break;case TrackProperty::Rotation:c.rotation=true;break;
                case TrackProperty::ScaleX:c.sx=true;break;case TrackProperty::ScaleY:c.sy=true;break;case TrackProperty::ScaleZ:c.sz=true;break;
                }
            }
        }
        result.assign(channels.begin(),channels.end());
    }
};
struct BoneClasses {
    std::vector<std::string> names;
    std::vector<bool> mechanism,aim;
    void update(const Skeleton& skeleton){
        bool same=names.size()==skeleton.bones.size();
        if(same)for(std::size_t i=0;i<names.size();++i)if(names[i]!=skeleton.bones[i].name){same=false;break;}
        if(same)return;
        names.clear();mechanism.clear();aim.clear();
        for(const auto& b:skeleton.bones){
            names.push_back(b.name);const auto n=canonicalName(b.name);
            mechanism.push_back(n.find("bolt")!=std::string::npos||n.find("clip")!=std::string::npos||n.find("magazine")!=std::string::npos||n.find("pump")!=std::string::npos||n.find("slide")!=std::string::npos||n.find("chamber")!=std::string::npos||n.find("shell")!=std::string::npos||n.find("bullet")!=std::string::npos||n.find("cylinder")!=std::string::npos);
            aim.push_back(n=="tag_torso"||n=="tag_ads");
        }
    }
};
struct Sample {
    const PoseLayer* node{};
    std::vector<Transform> pose,reference;
    std::vector<Channels> channels;
    float weight{};
};
struct Workspace {std::vector<Sample> samples;};
struct ThreadStorage {
    // Bounded clip metadata, no animation data ownership and no shared scene mutation.
    std::array<ChannelCache,64> channels;
    std::size_t next{},depth{};
    BoneClasses boneClasses;
    std::vector<std::unique_ptr<Workspace>> workspaces;
};
inline ThreadStorage& storage(){thread_local ThreadStorage value;return value;}
struct Lease {
    ThreadStorage& owner;Workspace& workspace;
    Lease():owner(storage()),workspace(acquire(owner)){}
    static Workspace& acquire(ThreadStorage& owner){
        if(owner.depth==owner.workspaces.size())owner.workspaces.push_back(std::make_unique<Workspace>());
        return *owner.workspaces[owner.depth++];
    }
    ~Lease(){--owner.depth;}
    Lease(const Lease&)=delete;
};
inline void markChannels(const Animation& animation,const Skeleton& skeleton,std::vector<Channels>& out,bool preserveMechanisms=false,bool preserveAim=false){
    auto& s=storage();ChannelCache* entry=nullptr;
    for(auto& c:s.channels)if(c.animation==&animation){entry=&c;break;}
    if(!entry)entry=&s.channels[s.next++%s.channels.size()];
    entry->get(animation,skeleton.bones.size(),out);
    if(preserveMechanisms||preserveAim){
        s.boneClasses.update(skeleton);
        for(std::size_t i=0;i<out.size();++i)if((preserveMechanisms&&s.boneClasses.mechanism[i])||(preserveAim&&s.boneClasses.aim[i]))out[i]={};
    }
}
}
}
