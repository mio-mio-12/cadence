#include "scene/ImportedBodyRetarget.h"
#include "scene/ImportedBodyMotion.h"
#include <iostream>
#include <cmath>
int main(int argc,char** argv){
    if(argc!=4){std::cerr<<"source_model.cast source_animation.cast target_model.cast\n";return 2;}
    try{
        auto source=std::make_shared<scene::CastScene>(scene::buildScene(cast::Document::load(argv[1]),false));
        const auto sourceIndex=source->animations.size();scene::appendAnimations(cast::Document::load(argv[2]),*source);
        scene::imported::repairDetachedBodyRootLoops(*source,sourceIndex);
        auto target=scene::buildScene(cast::Document::load(argv[3]),false);std::string error;
        if(sourceIndex>=source->animations.size()){std::cerr<<"No source clip bound\n";return 3;}
        auto index=target.animations.size();
        if(std::filesystem::path(argv[1])==std::filesystem::path(argv[3])){target=*source;index=sourceIndex;}
        else if(!scene::imported::appendRetargetedBody(target,source,sourceIndex,error)){std::cerr<<error<<'\n';return 4;}
        float maximumLengthError{};std::size_t invalid{};
        const auto layout=scene::imported::bodyOrCodLayout(target.skeleton);
        float rootTravel{},correctedTravel{},minDiagonal=1e9f,maxDiagonal{},minZ=1e9f,maxZ=-1e9f,maxLocalDelta{};std::string maxLocalBone;
        float maxVertexDelta{};int maxVertexFrame{};std::string maxVertexMesh,maxVertexBone;
        const auto start=target.samplePose(index,0);const auto origin=scene::transformPoint(start[layout.pelvis],{});
        for(std::size_t b=0;b<target.skeleton.bones.size();++b)if(target.skeleton.bones[b].parent<0||static_cast<int>(b)==layout.pelvis){const auto& bone=target.skeleton.bones[b];std::cout<<"anchor="<<bone.name<<" parent="<<bone.parent<<" restZ="<<bone.restGlobal.v[14]<<" poseZ="<<start[b].v[14]<<" restLocalZ="<<bone.restLocal.position.z<<'\n';}
        for(int i=0;i<=40;++i){const auto local=target.sampleLocalPose(index,target.animations[index].durationFrames*(i/40.f));
            for(std::size_t b=0;b<local.size();++b){if(static_cast<int>(b)!=layout.pelvis)maximumLengthError=std::max(maximumLengthError,std::abs(scene::length(local[b].position)-scene::length(target.skeleton.bones[b].restLocal.position)));}
            for(std::size_t b=0;b<local.size();++b)if(target.skeleton.bones[b].parent>=0&&static_cast<int>(b)!=layout.pelvis){const auto change=scene::length(local[b].position-target.skeleton.bones[b].restLocal.position);if(change>maxLocalDelta){maxLocalDelta=change;maxLocalBone=target.skeleton.bones[b].name;}}
            auto pose=target.globalPose(local);auto p=scene::transformPoint(pose[layout.pelvis],{})-origin;rootTravel=std::max(rootTravel,std::hypot(p.x,p.y));
            scene::imported::suppressBodyRootMotion(target.skeleton,pose);p=scene::transformPoint(pose[layout.pelvis],{})-scene::transformPoint(target.skeleton.bones[layout.pelvis].restGlobal,{});correctedTravel=std::max(correctedTravel,std::hypot(p.x,p.y));
            scene::Vec3 lo{1e9f,1e9f,1e9f},hi{-1e9f,-1e9f,-1e9f};
for(const auto& mesh:target.meshes)for(const auto& v:mesh.vertices){scene::Vec3 point{};float sum{};if(mesh.skinned)for(std::size_t k=0;k<v.weights.size();++k)if(v.weights[k]>0&&v.bones[k]<pose.size()){point+=scene::transformPoint(pose[v.bones[k]]*target.skeleton.bones[v.bones[k]].inverseBind,v.position)*v.weights[k];sum+=v.weights[k];}point=scene::transformPoint(mesh.modelTransform,sum>0?point/sum:v.position);const auto vertexDelta=scene::length(point-scene::transformPoint(mesh.modelTransform,v.position));if(vertexDelta>maxVertexDelta){maxVertexDelta=vertexDelta;maxVertexMesh=mesh.name;maxVertexFrame=i;float weight{};for(std::size_t k=0;k<v.weights.size();++k)if(v.weights[k]>weight&&v.bones[k]<target.skeleton.bones.size()){weight=v.weights[k];maxVertexBone=target.skeleton.bones[v.bones[k]].name;}}lo={std::min(lo.x,point.x),std::min(lo.y,point.y),std::min(lo.z,point.z)};hi={std::max(hi.x,point.x),std::max(hi.y,point.y),std::max(hi.z,point.z)};}
            const auto diagonal=scene::length(hi-lo);minDiagonal=std::min(minDiagonal,diagonal);maxDiagonal=std::max(maxDiagonal,diagonal);minZ=std::min(minZ,lo.z);maxZ=std::max(maxZ,lo.z);
            for(const auto& m:pose)for(const float v:m.v)invalid+=!std::isfinite(v);
        }
        std::size_t mapped{};if(const auto adapter=target.animations[index].coldWarWorldPose)for(const auto bone:adapter->sourceBones)mapped+=bone>=0;
        const auto end=std::nextafter(static_cast<float>(target.animations[index].durationFrames),0.f);const auto seconds=end/std::max(1.f,target.animations[index].framerate);
        const auto finalPose=target.samplePose(index,end);for(std::size_t b=0;b<target.skeleton.bones.size();++b)if(target.skeleton.bones[b].parent<0)std::cout<<"root_endpoint="<<target.skeleton.bones[b].name<<" start="<<start[b].v[12]<<','<<start[b].v[13]<<','<<start[b].v[14]<<" end="<<finalPose[b].v[12]<<','<<finalPose[b].v[13]<<','<<finalPose[b].v[14]<<'\n';
        std::cout<<"root_xy_range="<<rootTravel<<" corrected_xy_range="<<correctedTravel<<" bounds_diagonal_min="<<minDiagonal<<" max="<<maxDiagonal<<" feet_z_min="<<minZ<<" max="<<maxZ<<" rest_min_z="<<target.bounds.minimum.z<<" speed="<<scene::imported::bodyRootTravelSpeed(target.skeleton,start,target.samplePose(index,end),seconds)<<'\n';
        std::cout<<"mapped="<<mapped<<" target_bones="<<target.skeleton.bones.size()<<" source_unmapped_curves="<<source->animations[sourceIndex].unmappedCurveCount<<" nonfinite="<<invalid<<" max_length_error="<<maximumLengthError<<'\n';
        std::cout<<"max_child_translation_delta="<<maxLocalDelta<<" bone="<<maxLocalBone<<'\n';
        std::cout<<"max_vertex_delta="<<maxVertexDelta<<" mesh="<<maxVertexMesh<<" dominant_bone="<<maxVertexBone<<" phase="<<maxVertexFrame/40.f<<'\n';
        // Native clips may author translations on arbitrary helper bones.
        // Limb-length preservation is a retarget invariant, not a native one.
        return invalid||(mapped&&maximumLengthError>.001f)?1:0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
