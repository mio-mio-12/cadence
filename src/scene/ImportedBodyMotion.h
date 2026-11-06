#pragma once
#include "scene/ImportedBody.h"
namespace scene::imported {
// Recover omitted gait direction from stance-foot travel, never from a file hash.
// Named directions remain authoritative; ambiguous or incomplete cycles stay unknown.
inline void inferNativeWalkDirection(CastScene& scene,std::size_t index){
    if(index>=scene.animations.size())return;
    auto& clip=scene.animations[index];
    if(clip.sourceGame!="cso2"||clip.domain!=AnimationDomain::PlayerBody||clip.action!=ActionRole::None||clip.motion!=MotionRole::Walk||clip.direction!=Direction::Any||clip.durationFrames<4)return;
    const auto layout=bodyLayout(scene.skeleton);
    if(layout.family!=BodyFamily::Cso2||layout.pelvis<0||layout.foot[0]<0||layout.foot[1]<0)return;
    std::array<std::array<Vec3,21>,2> feet{};std::array<float,2> low{1e20f,1e20f};
    const auto position=[](const Mat4& m){return Vec3{m.v[12],m.v[13],m.v[14]};};
    const float legHeight=std::abs(position(scene.skeleton.bones[layout.pelvis].restGlobal).z-position(scene.skeleton.bones[layout.foot[0]].restGlobal).z);
    if(legHeight<1e-4f)return;
    for(int k=0;k<21;++k){const auto pose=scene.samplePose(index,clip.durationFrames*k/20.f);const auto pelvis=position(pose[layout.pelvis]);
        for(int side=0;side<2;++side){const auto foot=position(pose[layout.foot[side]]);feet[side][k]={foot.x-pelvis.x,foot.y-pelvis.y,foot.z};low[side]=std::min(low[side],foot.z);}}
    Vec3 travel{};float distance=0;int contacts=0;std::vector<Vec3> intervals;
    for(int side=0;side<2;++side)for(int k=1;k<20;++k){const auto a=feet[side][k-1],b=feet[side][k];
        if(a.z>low[side]+legHeight*.025f||b.z>low[side]+legHeight*.025f)continue;
        const Vec3 delta{a.x-b.x,a.y-b.y,0};travel=travel+delta;distance+=length(delta);++contacts;intervals.push_back(delta);}
    if(distance>0&&length(travel)/distance<.7f){
        // A recovering foot may skim the floor. Require a two-thirds directional
        // consensus before excluding those opposing low-foot intervals.
        std::array<int,8> votes{};std::array<Vec3,8> sums{};std::array<float,8> distances{};int moving=0;
        for(const auto delta:intervals){const float span=length(delta);if(span<legHeight*.005f)continue;
            const int sector=(static_cast<int>(std::round(std::atan2(delta.y,delta.x)/(kPi*.25f)))+8)%8;
            ++moving;++votes[sector];sums[sector]=sums[sector]+delta;distances[sector]+=span;}
        const auto winner=static_cast<std::size_t>(std::max_element(votes.begin(),votes.end())-votes.begin());
        if(votes[winner]<5||votes[winner]*3<moving*2)return;
        travel=sums[winner];distance=distances[winner];contacts=votes[winner];
    }
    if(contacts<5||length(travel)<legHeight*.15f||distance<=0||length(travel)/distance<.7f)return;
    const auto forward=layout.authoredForward;const Vec3 left{-forward.y,forward.x,0};
    const float angle=std::atan2(dot(travel,left),dot(travel,forward));
    const int sector=static_cast<int>(std::round(angle/(kPi*.25f)));
    if(std::abs(angle-sector*kPi*.25f)>kPi/9.f)return;
    constexpr std::array<Direction,8> directions{Direction::Forward,Direction::ForwardLeft,Direction::Left,Direction::BackwardLeft,Direction::Backward,Direction::BackwardRight,Direction::Right,Direction::ForwardRight};
    clip.direction=directions[(sector+8)%8];
}
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
    if(legacy==skeleton.boneByCanonicalName.end())return {};
    const int root=static_cast<int>(legacy->second);
    const auto pelvis=skeleton.boneByCanonicalName.find("j_mainroot");
    // Some canonicalized rigs put their origin at the pelvis. Retargeting
    // gives that coincident ancestor the pelvis translation, including collapse.
    // A distinct ground-level motion root must retain the old Z suppression.
    if(pelvis!=skeleton.boneByCanonicalName.end()&&pelvis->second<skeleton.bones.size()){
        const auto position=transformPoint(skeleton.bones[pelvis->second].restGlobal,{});
        int b=static_cast<int>(pelvis->second);
        for(std::size_t visited=0;visited<skeleton.bones.size()&&b>=0&&static_cast<std::size_t>(b)<skeleton.bones.size();++visited){
            if(length(transformPoint(skeleton.bones[b].restGlobal,{})-position)>.001f)break;
            if(b==root)return {root,true};
            b=skeleton.bones[b].parent;
        }
    }
    return {root,false};
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
