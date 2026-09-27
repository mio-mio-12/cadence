#pragma once
#include "scene/ImportedBody.h"
namespace scene::imported {
inline void repairDetachedBodyRootLoops(CastScene& scene,std::size_t first){
    const auto layout=bodyLayout(scene.skeleton);if(!layout||layout.pelvis<0)return;
    int mainRoot=layout.pelvis;while(scene.skeleton.bones[mainRoot].parent>=0)mainRoot=scene.skeleton.bones[mainRoot].parent;
    std::vector<bool> weighted(scene.skeleton.bones.size());
    for(const auto& mesh:scene.meshes)if(mesh.skinned)for(const auto& vertex:mesh.vertices)for(std::size_t k=0;k<vertex.weights.size();++k)if(vertex.weights[k]>0&&vertex.bones[k]<weighted.size()){
        int root=vertex.bones[k];while(scene.skeleton.bones[root].parent>=0)root=scene.skeleton.bones[root].parent;weighted[root]=true;
    }
    const float threshold=std::max(20.f,scene.bounds.valid?(scene.bounds.maximum.z-scene.bounds.minimum.z)*.25f:40.f);
    for(std::size_t a=first;a<scene.animations.size();++a){auto& clip=scene.animations[a];
        if(!clip.looping||clip.domain!=AnimationDomain::PlayerBody||clip.action!=ActionRole::None||clip.coldWarWorldPose||clip.durationFrames<1||
           (clip.motion!=MotionRole::Walk&&clip.motion!=MotionRole::Run&&clip.motion!=MotionRole::Sprint&&clip.motion!=MotionRole::Crawl))continue;
        const float duration=static_cast<float>(clip.durationFrames),end=std::nextafter(duration,0.f);
        const auto beginPose=scene.samplePose(a,0),endPose=scene.samplePose(a,end);
        const auto delta=[&](int b){return Vec3{endPose[b].v[12]-beginPose[b].v[12],endPose[b].v[13]-beginPose[b].v[13],0};};const auto mainDelta=delta(mainRoot);
        for(std::size_t b=0;b<weighted.size();++b)if(weighted[b]&&static_cast<int>(b)!=mainRoot&&scene.skeleton.bones[b].parent<0){
            const auto residual=delta(static_cast<int>(b))-mainDelta;if(length(residual)<threshold)continue;
            // A detached deform root must close with the body at a gait seam.
            // Remove only the mismatched linear ramp; preserve its oscillation.
            for(auto& track:clip.tracks)if(track.boneIndex==b&&track.mode==TrackMode::Absolute&&(track.property==TrackProperty::TranslationX||track.property==TrackProperty::TranslationY)){
                const float drift=track.property==TrackProperty::TranslationX?residual.x:residual.y;
                for(std::size_t k=0;k<track.scalarValues.size()&&k<track.frames.size();++k)track.scalarValues[k]-=drift*std::clamp(static_cast<float>(track.frames[k])/duration,0.f,1.f);
            }
        }
    }
}
// Native foreign rigs must not acquire CoD aliases merely to participate in
// movement extraction. Their pelvis owns body height, so never remove its Z.
struct BodyMotionRoot { int bone{-1};bool preserveVertical{}; };
inline BodyMotionRoot bodyMotionRoot(const Skeleton& skeleton){
    const auto imported=bodyLayout(skeleton);
    if(imported&&imported.pelvis>=0)return {imported.pelvis,true};
    const auto legacy=skeleton.boneByCanonicalName.find("tag_origin");
    return legacy==skeleton.boneByCanonicalName.end()?BodyMotionRoot{}:BodyMotionRoot{static_cast<int>(legacy->second),false};
}
inline void suppressBodyRootMotion(const Skeleton& skeleton,std::vector<Mat4>& pose,bool preserveVertical=false){
    const auto root=bodyMotionRoot(skeleton);if(root.bone<0||static_cast<std::size_t>(root.bone)>=pose.size())return;
    const auto& sampled=pose[root.bone];const auto& rest=skeleton.bones[root.bone].restGlobal;
    const Vec3 delta{sampled.v[12]-rest.v[12],sampled.v[13]-rest.v[13],root.preserveVertical||preserveVertical?0.f:sampled.v[14]-rest.v[14]};
    for(auto& bone:pose){bone.v[12]-=delta.x;bone.v[13]-=delta.y;bone.v[14]-=delta.z;}
}
inline float bodyRootTravelSpeed(const Skeleton& skeleton,const std::vector<Mat4>& first,const std::vector<Mat4>& last,float seconds){
    const auto root=bodyMotionRoot(skeleton);if(root.bone<0||static_cast<std::size_t>(root.bone)>=first.size()||static_cast<std::size_t>(root.bone)>=last.size()||seconds<=.001f)return 0.f;
    const float x=last[root.bone].v[12]-first[root.bone].v[12],y=last[root.bone].v[13]-first[root.bone].v[13];
    const float speed=std::sqrt(x*x+y*y)/seconds;return std::isfinite(speed)?speed:0.f;
}
}
