#pragma once
// Frozen v131 layering evaluator: comparison oracle only, never linked into the app.
#include "scene/CastScene.h"
#include <algorithm>
#include <cmath>
namespace scene {
std::string canonicalName(std::string value);
struct ReferenceSceneV131 : CastScene {
std::vector<Mat4> sampleLayeredPose(std::size_t,float,std::size_t,float,float,LayerMode,bool,const std::vector<Transform>* =nullptr) const;
std::vector<Mat4> sampleLayerStack(std::size_t,float,const std::vector<PoseLayer>&) const;
std::vector<Transform> sampleLocalPoseSlots(std::size_t,float,const std::vector<PoseSlot>&) const;
std::vector<Mat4> samplePoseSlots(std::size_t,float,const std::vector<PoseSlot>&) const;
};
std::vector<Mat4> ReferenceSceneV131::sampleLayeredPose(std::size_t baseAnimation,float baseFrame,
                                               std::size_t layerAnimation,float layerFrame,
                                               float weight,LayerMode mode,bool suppressRootMotion,const std::vector<Transform>* baseOverride) const {
    auto base=baseOverride?*baseOverride:sampleLocalPose(baseAnimation,baseFrame);
    if(layerAnimation>=animations.size()||base.size()!=skeleton.bones.size())return globalPose(base);
    const auto layer=sampleLocalPose(layerAnimation,layerFrame),reference=mode==LayerMode::Additive?sampleLocalPose(layerAnimation,0.0f):std::vector<Transform>{};weight=std::clamp(weight,0.0f,1.0f);
    struct Channels { bool tx{},ty{},tz{},rotation{},sx{},sy{},sz{}; };
    std::vector<Channels> animated(base.size());
    for(const auto& track:animations[layerAnimation].tracks)if(track.ownsLayer&&track.boneIndex<animated.size()){
        auto& channels=animated[track.boneIndex];
        switch(track.property){case TrackProperty::TranslationX:channels.tx=true;break;case TrackProperty::TranslationY:channels.ty=true;break;
            case TrackProperty::TranslationZ:channels.tz=true;break;case TrackProperty::Rotation:channels.rotation=true;break;
            case TrackProperty::ScaleX:channels.sx=true;break;case TrackProperty::ScaleY:channels.sy=true;break;case TrackProperty::ScaleZ:channels.sz=true;break;}
    }
    for(std::size_t i=0;i<base.size()&&i<layer.size();++i){
        if(suppressRootMotion&&skeleton.bones[i].parent<0)continue;
        const auto& channels=animated[i];
        if(mode==LayerMode::Override){
            if(channels.tx)base[i].position.x=base[i].position.x+(layer[i].position.x-base[i].position.x)*weight;
            if(channels.ty)base[i].position.y=base[i].position.y+(layer[i].position.y-base[i].position.y)*weight;
            if(channels.tz)base[i].position.z=base[i].position.z+(layer[i].position.z-base[i].position.z)*weight;
            if(channels.rotation)base[i].rotation=slerp(base[i].rotation,layer[i].rotation,weight);
            if(channels.sx)base[i].scale.x=base[i].scale.x+(layer[i].scale.x-base[i].scale.x)*weight;
            if(channels.sy)base[i].scale.y=base[i].scale.y+(layer[i].scale.y-base[i].scale.y)*weight;
            if(channels.sz)base[i].scale.z=base[i].scale.z+(layer[i].scale.z-base[i].scale.z)*weight;
            continue;
        }
        const auto& origin=reference[i];
        if(channels.tx)base[i].position.x+=(layer[i].position.x-origin.position.x)*weight;
        if(channels.ty)base[i].position.y+=(layer[i].position.y-origin.position.y)*weight;
        if(channels.tz)base[i].position.z+=(layer[i].position.z-origin.position.z)*weight;
        if(channels.rotation){
            const Quat inverseOrigin{-origin.rotation.x,-origin.rotation.y,-origin.rotation.z,origin.rotation.w};
            const auto rotationDelta=normalize(multiply(inverseOrigin,layer[i].rotation));
            base[i].rotation=normalize(multiply(base[i].rotation,slerp(Quat{},rotationDelta,weight)));
        }
        if(channels.sx){const float ratio=layer[i].scale.x/std::max(std::abs(origin.scale.x),1e-8f);base[i].scale.x*=1.0f+(ratio-1.0f)*weight;}
        if(channels.sy){const float ratio=layer[i].scale.y/std::max(std::abs(origin.scale.y),1e-8f);base[i].scale.y*=1.0f+(ratio-1.0f)*weight;}
        if(channels.sz){const float ratio=layer[i].scale.z/std::max(std::abs(origin.scale.z),1e-8f);base[i].scale.z*=1.0f+(ratio-1.0f)*weight;}
    }
    return globalPose(base);
}

std::vector<Mat4> ReferenceSceneV131::sampleLayerStack(std::size_t baseAnimation,float baseFrame,const std::vector<PoseLayer>& layers) const {
    auto base=sampleLocalPose(baseAnimation,baseFrame);struct Channels { bool tx{},ty{},tz{},rotation{},sx{},sy{},sz{}; };
    for(const auto& entry:layers){if(entry.animation>=animations.size()||entry.weight<=0)continue;const auto layer=sampleLocalPose(entry.animation,entry.frame),reference=sampleLocalPose(entry.animation,0.0f);const float weight=std::clamp(entry.weight,0.0f,1.0f);std::vector<Channels> animated(base.size());
        for(const auto& track:animations[entry.animation].tracks)if(track.ownsLayer&&track.boneIndex<animated.size()){auto& channels=animated[track.boneIndex];switch(track.property){case TrackProperty::TranslationX:channels.tx=true;break;case TrackProperty::TranslationY:channels.ty=true;break;case TrackProperty::TranslationZ:channels.tz=true;break;case TrackProperty::Rotation:channels.rotation=true;break;case TrackProperty::ScaleX:channels.sx=true;break;case TrackProperty::ScaleY:channels.sy=true;break;case TrackProperty::ScaleZ:channels.sz=true;break;}}
        for(std::size_t i=0;i<base.size()&&i<layer.size();++i){if(entry.suppressRootMotion&&skeleton.bones[i].parent<0)continue;const auto& channels=animated[i];const auto& origin=reference[i];if(entry.mode==LayerMode::Override){if(channels.tx)base[i].position.x+=(layer[i].position.x-base[i].position.x)*weight;if(channels.ty)base[i].position.y+=(layer[i].position.y-base[i].position.y)*weight;if(channels.tz)base[i].position.z+=(layer[i].position.z-base[i].position.z)*weight;if(channels.rotation)base[i].rotation=slerp(base[i].rotation,layer[i].rotation,weight);if(channels.sx)base[i].scale.x+=(layer[i].scale.x-base[i].scale.x)*weight;if(channels.sy)base[i].scale.y+=(layer[i].scale.y-base[i].scale.y)*weight;if(channels.sz)base[i].scale.z+=(layer[i].scale.z-base[i].scale.z)*weight;continue;}
            if(channels.tx)base[i].position.x+=(layer[i].position.x-origin.position.x)*weight;if(channels.ty)base[i].position.y+=(layer[i].position.y-origin.position.y)*weight;if(channels.tz)base[i].position.z+=(layer[i].position.z-origin.position.z)*weight;if(channels.rotation){const Quat inverseOrigin{-origin.rotation.x,-origin.rotation.y,-origin.rotation.z,origin.rotation.w};const auto delta=normalize(multiply(inverseOrigin,layer[i].rotation));base[i].rotation=normalize(multiply(base[i].rotation,slerp(Quat{},delta,weight)));}if(channels.sx)base[i].scale.x*=1.0f+(layer[i].scale.x/std::max(std::abs(origin.scale.x),1e-8f)-1.0f)*weight;if(channels.sy)base[i].scale.y*=1.0f+(layer[i].scale.y/std::max(std::abs(origin.scale.y),1e-8f)-1.0f)*weight;if(channels.sz)base[i].scale.z*=1.0f+(layer[i].scale.z/std::max(std::abs(origin.scale.z),1e-8f)-1.0f)*weight;}}
    return globalPose(base);
}

std::vector<Transform> ReferenceSceneV131::sampleLocalPoseSlots(std::size_t baseAnimation,float baseFrame,const std::vector<PoseSlot>& slots) const {
    auto base=sampleLocalPose(baseAnimation,baseFrame);
    struct Channels { bool tx{},ty{},tz{},rotation{},sx{},sy{},sz{}; };
    const auto mechanismBone=[](std::string name){name=canonicalName(std::move(name));return name.find("bolt")!=std::string::npos||name.find("clip")!=std::string::npos||name.find("magazine")!=std::string::npos||name.find("pump")!=std::string::npos||name.find("slide")!=std::string::npos||name.find("chamber")!=std::string::npos||name.find("shell")!=std::string::npos||name.find("bullet")!=std::string::npos||name.find("cylinder")!=std::string::npos;};
    const auto markChannels=[&](const Animation& animation,std::vector<Channels>& channels,bool preserveMechanisms,bool preserveAimRoot){
        for(const auto& track:animation.tracks)if(track.ownsLayer&&track.boneIndex<channels.size()){
            if(preserveMechanisms&&track.boneIndex<skeleton.bones.size()&&mechanismBone(skeleton.bones[track.boneIndex].name))continue;
            if(preserveAimRoot&&track.boneIndex<skeleton.bones.size()){const auto aimRoot=canonicalName(skeleton.bones[track.boneIndex].name);if(aimRoot=="tag_torso"||aimRoot=="tag_ads")continue;}
            auto& owned=channels[track.boneIndex];
            switch(track.property){case TrackProperty::TranslationX:owned.tx=true;break;case TrackProperty::TranslationY:owned.ty=true;break;
                case TrackProperty::TranslationZ:owned.tz=true;break;case TrackProperty::Rotation:owned.rotation=true;break;
                case TrackProperty::ScaleX:owned.sx=true;break;case TrackProperty::ScaleY:owned.sy=true;break;case TrackProperty::ScaleZ:owned.sz=true;break;}
        }
    };
    for(const auto& slot:slots){
        struct Sample { const PoseLayer* node{};std::vector<Transform> pose,reference;std::vector<Channels> channels;float weight{}; };
        std::vector<Sample> samples;samples.reserve(slot.nodes.size());
        for(const auto& node:slot.nodes){if(node.animation>=animations.size()||node.weight<=0)continue;Sample sample;sample.node=&node;sample.pose=sampleLocalPose(node.animation,node.frame);if(node.mode==LayerMode::Additive)sample.reference=sampleLocalPose(node.referenceAnimation<animations.size()?node.referenceAnimation:node.animation,node.referenceAnimation<animations.size()?node.referenceFrame:0);sample.channels.resize(base.size());sample.weight=std::max(0.0f,node.weight);markChannels(animations[node.animation],sample.channels,node.preserveWeaponMechanisms,node.preserveViewmodelAimRoot);samples.push_back(std::move(sample));}
        if(samples.empty())continue;
        for(std::size_t bone=0;bone<base.size();++bone){
            const auto channelWeight=[&](auto member){float total{};for(const auto& sample:samples)if(sample.channels[bone].*member&&!(sample.node->suppressRootMotion&&skeleton.bones[bone].parent<0)&&sample.node->mode==LayerMode::Override)total+=sample.weight;return total;};
            const auto blendScalar=[&](float& target,auto member,auto value){const float total=channelWeight(member);if(total<=0)return;const float norm=std::max(1.0f,total);float result=target*(1.0f-std::min(1.0f,total));for(const auto& sample:samples)if(sample.channels[bone].*member&&!(sample.node->suppressRootMotion&&skeleton.bones[bone].parent<0)&&sample.node->mode==LayerMode::Override)result+=value(sample.pose[bone])*(sample.weight/norm);target=result;};
            blendScalar(base[bone].position.x,&Channels::tx,[](const Transform& value){return value.position.x;});
            blendScalar(base[bone].position.y,&Channels::ty,[](const Transform& value){return value.position.y;});
            blendScalar(base[bone].position.z,&Channels::tz,[](const Transform& value){return value.position.z;});
            blendScalar(base[bone].scale.x,&Channels::sx,[](const Transform& value){return value.scale.x;});
            blendScalar(base[bone].scale.y,&Channels::sy,[](const Transform& value){return value.scale.y;});
            blendScalar(base[bone].scale.z,&Channels::sz,[](const Transform& value){return value.scale.z;});
            const float rotationWeight=channelWeight(&Channels::rotation);if(rotationWeight>0){const float norm=std::max(1.0f,rotationWeight);Quat result=base[bone].rotation;float accumulated=std::max(0.0f,1.0f-std::min(1.0f,rotationWeight));for(const auto& sample:samples)if(sample.channels[bone].rotation&&!(sample.node->suppressRootMotion&&skeleton.bones[bone].parent<0)&&sample.node->mode==LayerMode::Override){const float contribution=sample.weight/norm;result=slerp(result,sample.pose[bone].rotation,contribution/std::max(1e-8f,accumulated+contribution));accumulated+=contribution;}base[bone].rotation=result;}
            for(const auto& sample:samples){if(sample.node->mode!=LayerMode::Additive||sample.node->suppressRootMotion&&skeleton.bones[bone].parent<0)continue;const auto& owned=sample.channels[bone];const auto& value=sample.pose[bone];const auto& origin=sample.reference[bone];const float weight=std::clamp(sample.weight,0.0f,1.0f);
                if(owned.tx)base[bone].position.x+=(value.position.x-origin.position.x)*weight;if(owned.ty)base[bone].position.y+=(value.position.y-origin.position.y)*weight;if(owned.tz)base[bone].position.z+=(value.position.z-origin.position.z)*weight;
                if(owned.rotation){const Quat inverseOrigin{-origin.rotation.x,-origin.rotation.y,-origin.rotation.z,origin.rotation.w};const auto delta=normalize(multiply(inverseOrigin,value.rotation));base[bone].rotation=normalize(multiply(base[bone].rotation,slerp(Quat{},delta,weight)));}
                if(owned.sx)base[bone].scale.x*=1.0f+(value.scale.x/std::max(std::abs(origin.scale.x),1e-8f)-1.0f)*weight;if(owned.sy)base[bone].scale.y*=1.0f+(value.scale.y/std::max(std::abs(origin.scale.y),1e-8f)-1.0f)*weight;if(owned.sz)base[bone].scale.z*=1.0f+(value.scale.z/std::max(std::abs(origin.scale.z),1e-8f)-1.0f)*weight;
            }
        }
    }
    return base;
}

std::vector<Mat4> ReferenceSceneV131::samplePoseSlots(std::size_t baseAnimation,float baseFrame,const std::vector<PoseSlot>& slots) const {
    return globalPose(sampleLocalPoseSlots(baseAnimation,baseFrame,slots));
}

}

