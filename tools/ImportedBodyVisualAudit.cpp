#include "scene/ImportedBodyRetarget.h"
#include "scene/ImportedBodyMotion.h"
#include "render/StageRenderer.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstdlib>
int main(int argc,char** argv){
    if(argc!=5){std::cerr<<"source_model source_clip target_model output_folder\n";return 2;}
    try{
        auto donor=std::make_shared<scene::CastScene>(scene::buildScene(cast::Document::load(argv[1]),false));
        const auto sourceIndex=donor->animations.size();scene::appendAnimations(cast::Document::load(argv[2]),*donor);
        scene::imported::repairDetachedBodyRootLoops(*donor,sourceIndex);
        if(sourceIndex>=donor->animations.size()){std::cerr<<"No source clip\n";return 3;}
        const auto layout=scene::imported::bodyOrCodLayout(donor->skeleton);
        if(std::getenv("CADENCE_AUDIT_UNROTATE_SOURCE")&&scene::imported::sourceBodyFamily(layout.family)){
            const auto inverse=scene::fromEulerRadians({0,0,-scene::kPi*.5f});
            for(auto& track:donor->animations[sourceIndex].tracks)if(donor->skeleton.bones[track.boneIndex].parent<0){
                if(track.property==scene::TrackProperty::Rotation)for(auto& q:track.rotationValues)q=scene::multiply(inverse,q);
                else if(track.property==scene::TrackProperty::TranslationX){track.property=scene::TrackProperty::TranslationY;for(auto& v:track.scalarValues)v=-v;}
                else if(track.property==scene::TrackProperty::TranslationY)track.property=scene::TrackProperty::TranslationX;
            }
        }
        if(layout.thigh[0]>=0&&layout.thigh[1]>=0){const auto pose=donor->samplePose(sourceIndex,0);const auto left=scene::transformPoint(pose[layout.thigh[0]],{})-scene::transformPoint(pose[layout.thigh[1]],{});const auto facing=scene::normalize(scene::cross(left,scene::Vec3{0,0,1}));std::cout<<"native_bind_facing="<<layout.authoredForward.x<<','<<layout.authoredForward.y<<" native_pose_facing="<<facing.x<<','<<facing.y<<" scale="<<donor->importedTranslationScale<<'\n';}
        auto target=scene::buildScene(cast::Document::load(argv[3]),false);std::string error;
        auto index=target.animations.size();if(std::filesystem::path(argv[1])==std::filesystem::path(argv[3])){target=*donor;index=sourceIndex;}
        else if(!scene::imported::appendRetargetedBody(target,donor,sourceIndex,error)){std::cerr<<error;return 4;}
        if(!glfwInit())return 5;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(960,720,"Imported body retarget visual audit",nullptr,nullptr);if(!window)return 5;glfwMakeContextCurrent(window);
        render::StageRenderer renderer;if(!renderer.initialize(error)||!renderer.loadScene(target,error)){std::cerr<<error;return 6;}renderer.setDebugView(1);renderer.setViewmodelCapture(true,{.09f,.10f,.12f,1});
        const std::filesystem::path folder=argv[4];std::filesystem::create_directories(folder);
        for(const auto fraction:{0.f,.5f,.95f}){
            const auto pose=target.samplePose(index,target.animations[index].durationFrames*fraction);scene::Vec3 minimum{1e9f,1e9f,1e9f},maximum{-1e9f,-1e9f,-1e9f};
            for(const auto& mesh:target.meshes)for(const auto& vertex:mesh.vertices){scene::Vec3 p{};float sum{};
                if(mesh.skinned)for(std::size_t k=0;k<vertex.weights.size();++k)if(vertex.weights[k]>0&&vertex.bones[k]<pose.size()){p+=scene::transformPoint(pose[vertex.bones[k]]*target.skeleton.bones[vertex.bones[k]].inverseBind,vertex.position)*vertex.weights[k];sum+=vertex.weights[k];}
                if(sum<=0)p=vertex.position;else p=p/sum;p=scene::transformPoint(mesh.modelTransform,p);
                minimum={std::min(minimum.x,p.x),std::min(minimum.y,p.y),std::min(minimum.z,p.z)};maximum={std::max(maximum.x,p.x),std::max(maximum.y,p.y),std::max(maximum.z,p.z)};
            }
            const auto center=(minimum+maximum)*.5f;const float extent=std::max(10.f,scene::length(maximum-minimum));
            const auto forward=scene::imported::bodyOrCodLayout(target.skeleton).authoredForward;
            const auto eye=center+(forward+scene::cross(forward,scene::Vec3{0,0,1})*.5f)*extent+scene::Vec3{0,0,extent*.13f};
            const auto vp=scene::perspective(50*scene::kPi/180,960.f/720.f,.1f,10000)*scene::lookAt(eye,center,{0,0,1});renderer.render(target,pose,vp,960,720,false,false,false);
            if(!renderer.saveColorPng(folder/("body_"+std::to_string(fraction)+".png"),error)){std::cerr<<error;return 7;}
        }
        renderer.shutdown();glfwDestroyWindow(window);glfwTerminate();return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
