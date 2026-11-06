#include "scene/ImportedBodyRetarget.h"
#include "scene/ImportedBodyMotion.h"
#include "render/StageRenderer.h"
#include "gameplay/DeathRagdoll.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstdlib>
int main(int argc,char** argv){
    if(argc!=5){std::cerr<<"source_model source_clip target_model output_folder\n";return 2;}
    try{
        auto donor=std::make_shared<scene::CastScene>(scene::buildScene(cast::Document::load(argv[1]),false));
        const auto sourceIndex=donor->animations.size();
        if(std::string_view(argv[2])=="-"){scene::Animation rest;rest.name="Unmodified exported bind pose";rest.domain=scene::AnimationDomain::PlayerBody;rest.durationFrames=1;rest.framerate=30;donor->animations.push_back(rest);}
        else scene::appendAnimations(cast::Document::load(argv[2]),*donor);
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
        if(std::getenv("CADENCE_BODY_DISSOLVE")){renderer.setViewmodelCapture(false,{.09f,.1f,.12f,1});if(!renderer.loadActorScene(target,error))return 6;}
        const std::filesystem::path folder=argv[4];std::filesystem::create_directories(folder);
        std::vector<float> phases{0.f,.5f,.95f};
        if(std::getenv("CADENCE_BODY_DENSE")){phases.clear();for(int phase=0;phase<=20;++phase)phases.push_back(phase/20.f);}
        for(const auto fraction:phases){
            auto pose=target.samplePose(index,target.animations[index].durationFrames*fraction);
            if(std::getenv("CADENCE_BODY_RAGDOLL")){
                pose=target.samplePose(index,target.animations[index].durationFrames);
                const float lift=target.bounds.valid?-target.bounds.minimum.z:0;for(auto& m:pose)m=scene::translation({0,0,lift})*m;
                gameplay::ragdoll::State rag;gameplay::ragdoll::Settings settings;
                if(!rag.begin(target.skeleton,pose,settings))return 8;
                for(int step=0;step<int(fraction*480);++step)rag.advance(1.f/120,[](scene::Vec3,scene::Vec3 p,float r){p.z=std::max(p.z,r);return p;});
                rag.apply(pose);
            }
            scene::Vec3 minimum{1e9f,1e9f,1e9f},maximum{-1e9f,-1e9f,-1e9f};
            if(const auto& adapter=target.animations[index].coldWarWorldPose;adapter&&adapter->source){
                const auto nativePose=adapter->source->samplePose(adapter->animation,target.animations[index].durationFrames*fraction);
                const auto targetDigits=scene::imported::bodyFingerChains(target.skeleton),sourceDigits=scene::imported::bodyFingerChains(adapter->source->skeleton);
                const auto segment=[](const scene::Skeleton& rig,const std::vector<scene::Mat4>& p,const std::array<int,3>& chain,int joint){const int a=chain[joint];if(a<0)return scene::Vec3{};
                    if(joint<2&&chain[joint+1]>=0)return scene::transformPoint(p[chain[joint+1]],{})-scene::transformPoint(p[a],{});
                    if(joint==0||chain[joint-1]<0)return scene::Vec3{};const int previous=chain[joint-1];const auto localDirection=scene::transformPoint(scene::inverseAffine(rig.bones[previous].restGlobal),scene::transformPoint(rig.bones[a].restGlobal,{}));
                    return scene::transformPoint(p[a],localDirection)-scene::transformPoint(p[a],{});};
                for(int hand=0;hand<2;++hand)for(int digit=0;digit<5;++digit)for(int joint=0;joint<3;++joint){const int ta=targetDigits[hand][digit][joint],sa=sourceDigits[hand][digit][joint];if(ta<0||sa<0)continue;
                    const auto targetDirection=scene::normalize(segment(target.skeleton,pose,targetDigits[hand][digit],joint));
                    const auto sourceDirection=scene::normalize(scene::transformPoint(scene::rotation(adapter->actionBasis),segment(adapter->source->skeleton,nativePose,sourceDigits[hand][digit],joint)));
                    std::cout<<"DIGIT,"<<fraction<<','<<hand<<','<<digit<<','<<joint<<",cos="<<scene::dot(targetDirection,sourceDirection)<<",target="<<targetDirection.x<<','<<targetDirection.y<<','<<targetDirection.z<<",source="<<sourceDirection.x<<','<<sourceDirection.y<<','<<sourceDirection.z<<'\n';
                }
            }
            for(const auto& mesh:target.meshes)for(const auto& vertex:mesh.vertices){scene::Vec3 p{};float sum{};
                if(mesh.skinned)for(std::size_t k=0;k<vertex.weights.size();++k)if(vertex.weights[k]>0&&vertex.bones[k]<pose.size()){p+=scene::transformPoint(pose[vertex.bones[k]]*target.skeleton.bones[vertex.bones[k]].inverseBind,vertex.position)*vertex.weights[k];sum+=vertex.weights[k];}
                if(sum<=0)p=vertex.position;else p=p/sum;p=scene::transformPoint(mesh.modelTransform,p);
                minimum={std::min(minimum.x,p.x),std::min(minimum.y,p.y),std::min(minimum.z,p.z)};maximum={std::max(maximum.x,p.x),std::max(maximum.y,p.y),std::max(maximum.z,p.z)};
            }
            const auto center=(minimum+maximum)*.5f;const float extent=std::max(10.f,scene::length(maximum-minimum));
            const auto forward=scene::imported::bodyOrCodLayout(target.skeleton).authoredForward;
            const auto eye=center+(forward+scene::cross(forward,scene::Vec3{0,0,1})*.5f)*extent+scene::Vec3{0,0,extent*.13f};
            const auto vp=scene::perspective(50*scene::kPi/180,960.f/720.f,.1f,10000)*scene::lookAt(eye,center,{0,0,1});
            if(std::getenv("CADENCE_BODY_DISSOLVE")){std::vector<std::vector<scene::Mat4>> actors{pose};renderer.setActorDissolves({fraction});renderer.render(target,pose,vp,960,720,false,false,false,&target,&actors,nullptr,nullptr,nullptr,false);}
            else renderer.render(target,pose,vp,960,720,false,false,false);
            if(!renderer.saveColorPng(folder/("body_"+std::to_string(fraction)+".png"),error)){std::cerr<<error;return 7;}
            if(std::getenv("CADENCE_BODY_DETAIL"))for(int hand=0;hand<2;++hand){const auto b=scene::imported::bodyOrCodLayout(target.skeleton);if(b.hand[hand]<0)continue;
                const auto handCenter=scene::transformPoint(pose[b.hand[hand]],{});const auto handEye=handCenter+(forward+scene::cross(forward,scene::Vec3{0,0,1})*.5f)*30.f+scene::Vec3{0,0,15};
                const auto handVp=scene::perspective(40*scene::kPi/180,960.f/720.f,.1f,10000)*scene::lookAt(handEye,handCenter,{0,0,1});
                renderer.render(target,pose,handVp,960,720,false,false,false);
                if(!renderer.saveColorPng(folder/("hand"+std::to_string(hand)+"_"+std::to_string(fraction)+".png"),error)){std::cerr<<error;return 7;}}
        }
        renderer.shutdown();glfwDestroyWindow(window);glfwTerminate();return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
