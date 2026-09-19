#pragma once
#include "take/Take.h"
namespace take {
// Incremental accounting: never scan all old samples every rendered frame.
// Matrix counts include the player world rig and the actual per-slot poses.
struct MemoryUsage {
    std::size_t scanned{},poseBytes{},payloadBytes{};
    const scene::Mat4* firstPose{};
    void reset(){*this={};}
    static std::size_t strings(const RecordedActorState& a){return a.primaryClip.capacity()+a.torsoClip.capacity()+2;}
    void update(const Take& take){
        const auto* first=take.samples.empty()?nullptr:take.samples.front().pose.data();
        if(first!=firstPose||take.samples.size()<scanned){reset();firstPose=first;}
        for(;scanned<take.samples.size();++scanned){
            const auto& s=take.samples[scanned];
            const auto addPose=[&](const auto& pose){poseBytes+=pose.size()*sizeof(scene::Mat4);payloadBytes+=pose.capacity()*sizeof(scene::Mat4);};
            addPose(s.pose);addPose(s.worldActor.pose);
            payloadBytes+=s.bots.capacity()*sizeof(RecordedActorState)+s.hiddenBones.capacity()*sizeof(std::uint32_t)+
                s.primaryClip.capacity()+s.torsoClip.capacity()+2+strings(s.worldActor);
            for(const auto& b:s.bots){addPose(b.pose);payloadBytes+=strings(b);}
        }
    }
    std::size_t estimatedBytes(const Take& take)const{
        return sizeof(Take)+take.samples.capacity()*sizeof(Sample)+payloadBytes+take.shots.capacity()*sizeof(ShotEvent);
    }
};
}
