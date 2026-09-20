#include "assets/LocalAssetPaths.h"
#include "cast/CastDocument.h"
#include "render/StageRenderer.h"
#include "scene/CastScene.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct Clip {
    const char* family;
    const char* animation;
};

struct FrameBounds {
    scene::Vec3 minimum{};
    scene::Vec3 maximum{};
    bool valid{};
};

void writeRigWeights(const scene::CastScene& value,const std::filesystem::path& path) {
    std::vector<double> weights(value.skeleton.bones.size());std::vector<std::size_t> vertices(value.skeleton.bones.size());
    for(const auto& mesh:value.meshes)for(const auto& vertex:mesh.vertices)for(std::size_t influence=0;influence<vertex.bones.size();++influence)if(vertex.bones[influence]<weights.size()&&vertex.weights[influence]>0){weights[vertex.bones[influence]]+=vertex.weights[influence];++vertices[vertex.bones[influence]];}
    std::ofstream out(path);out<<"bone,parent,vertices,weight,rest_local_x,rest_local_y,rest_local_z,rest_global_x,rest_global_y,rest_global_z\n";
    for(std::size_t bone=0;bone<value.skeleton.bones.size();++bone){const auto& item=value.skeleton.bones[bone];out<<item.name<<','<<(item.parent>=0?value.skeleton.bones[static_cast<std::size_t>(item.parent)].name:"")<<','<<vertices[bone]<<','<<weights[bone]<<','<<item.restLocal.position.x<<','<<item.restLocal.position.y<<','<<item.restLocal.position.z<<','<<item.restGlobal.v[12]<<','<<item.restGlobal.v[13]<<','<<item.restGlobal.v[14]<<'\n';}
}

void include(FrameBounds& bounds,scene::Vec3 point) {
    if(!bounds.valid){bounds.minimum=bounds.maximum=point;bounds.valid=true;return;}
    bounds.minimum={std::min(bounds.minimum.x,point.x),std::min(bounds.minimum.y,point.y),std::min(bounds.minimum.z,point.z)};
    bounds.maximum={std::max(bounds.maximum.x,point.x),std::max(bounds.maximum.y,point.y),std::max(bounds.maximum.z,point.z)};
}

FrameBounds posedBounds(const scene::CastScene& value,const std::vector<scene::Mat4>& pose) {
    FrameBounds result;
    for(const auto& mesh:value.meshes)for(const auto& vertex:mesh.vertices){
        scene::Vec3 point{};float total{};
        if(mesh.skinned)for(std::size_t influence=0;influence<vertex.bones.size();++influence){
            const auto bone=vertex.bones[influence];const auto weight=vertex.weights[influence];
            if(weight<=0||bone>=pose.size()||bone>=value.skeleton.bones.size())continue;
            const auto skin=pose[bone]*value.skeleton.bones[bone].inverseBind;
            point=point+scene::transformPoint(skin,vertex.position)*weight;total+=weight;
        }
        if(!mesh.skinned||total<0.0001f)point=vertex.position;
        else point=point/total;
        include(result,scene::transformPoint(mesh.modelTransform,point));
    }
    return result;
}

scene::Mat4 comparisonCamera(const FrameBounds& bounds,scene::Vec3 direction,int width,int height,float worldScale=1.0f) {
    const scene::Vec3 center=(bounds.minimum+bounds.maximum)*0.5f*worldScale;
    const auto extent=bounds.maximum-bounds.minimum;
    const float radius=std::max({extent.x,extent.y,extent.z,1.0f})*0.7f*worldScale;
    direction=scene::normalize(direction);
    const scene::Vec3 up=std::abs(direction.z)>.9f?scene::Vec3{0,1,0}:scene::Vec3{0,0,1};
    const auto view=scene::lookAt(center+direction*radius*3.0f,center,up);
    const float aspect=static_cast<float>(width)/static_cast<float>(height);
    return scene::orthographic(-radius*aspect,radius*aspect,-radius,radius,0.01f,radius*8.0f)*view;
}

bool loadRig(const std::filesystem::path& handsPath,const std::filesystem::path& weaponPath,
             const std::filesystem::path& animationPath,float weaponScale,scene::CastScene& result,
             std::size_t& animationIndex) {
    const auto hands=cast::Document::load(handsPath),weapon=cast::Document::load(weaponPath),animation=cast::Document::load(animationPath);
    if(!hands.valid()||!weapon.valid()||!animation.valid())return false;
    result=scene::buildScene(hands);
    const auto firstWeaponMesh=result.meshes.size();
    if(!scene::appendRigModel(weapon,result,weaponPath.parent_path().filename().string(),scene::Mat4::identity(),weaponScale))return false;
    // Local matte materials keep anatomical comparisons free of texture detail.
    for(std::size_t index=0;index<result.meshes.size();++index){
        auto& mesh=result.meshes[index];
        mesh.albedoPath.clear();mesh.normalPath.clear();mesh.specularPath.clear();
        const float gray=index<firstWeaponMesh?0.65f:0.35f;
        mesh.color={gray,gray,gray,1.0f};
        mesh.gltfPbr=true;mesh.roughnessFactor=1.0f;mesh.metallicFactor=0.0f;
        mesh.emissive=false;mesh.lens=false;mesh.camoBlend=false;mesh.forceAlpha=false;mesh.alphaTest=false;
    }
    animationIndex=result.animations.size();scene::appendAnimations(animation,result);
    return animationIndex<result.animations.size();
}

bool saveFrame(render::StageRenderer& renderer,const scene::CastScene& value,const std::vector<scene::Mat4>& pose,
               const std::filesystem::path& path,const scene::Mat4& viewProjection) {
    constexpr int width=960,height=720;
    std::string error;if(!renderer.loadScene(value,error)){std::cerr<<"Renderer scene load failed: "<<error<<'\n';return false;}
    renderer.setDebugView(9);
    renderer.render(value,pose,viewProjection,width,height,false,false,false);
    if(!renderer.saveColorPng(path,error)){std::cerr<<"PNG save failed: "<<error<<'\n';return false;}
    return true;
}

scene::Mat4 firstPersonCamera(float worldScale) {
    // A shared fixed view avoids comparing different animated camera bones.
    // Scale the clipping distances with the same scene units as the geometry.
    return scene::perspective(90.0f*scene::kPi/180.0f,960.0f/720.0f,2.5f*worldScale,10000.0f*worldScale)*
           scene::lookAtDirection({0,0,0},{1,0,0},{0,0,1});
}

}

int main(int argc,char** argv) {
    const std::filesystem::path root=argc>1?std::filesystem::u8path(argv[1]):std::filesystem::path{cadence::local_assets::exportPath("")};
    const std::filesystem::path output=argc>2?std::filesystem::u8path(argv[2]):std::filesystem::current_path()/"pose_visual_regression";
    const auto sourceHandsPath=root/R"(cs2\models\viewhands\viewhands_model.cast)";
    const auto targetHandsPath=root/R"(bo2\models\viewhands\seal6\c_usa_mp_seal6_longsleeve_viewhands\c_usa_mp_seal6_longsleeve_viewhands_LOD0.cast)";
    const auto sourceHands=cast::Document::load(sourceHandsPath),targetHands=cast::Document::load(targetHandsPath);
    if(!sourceHands.valid()||!targetHands.valid()){std::cerr<<"Could not load authoritative hands for retarget scale\n";return 1;}
    const float targetScale=scene::source2RetargetScale(scene::buildScene(targetHands).skeleton,scene::buildScene(sourceHands).skeleton);
    std::filesystem::create_directories(output);
    if(!glfwInit()){std::cerr<<"GLFW initialization failed\n";return 1;}
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    auto* window=glfwCreateWindow(64,64,"Cadence pose visual regression",nullptr,nullptr);
    if(!window){glfwTerminate();std::cerr<<"Hidden OpenGL context creation failed\n";return 1;}
    glfwMakeContextCurrent(window);render::StageRenderer renderer;std::string error;
    if(!renderer.initialize(error)){std::cerr<<"Renderer initialization failed: "<<error<<'\n';glfwDestroyWindow(window);glfwTerminate();return 1;}
    renderer.setViewmodelCapture(true,{0.025f,0.025f,0.03f,1.0f});renderer.setAntialiasing(true);renderer.setIgnoreTextureAlpha(true,false);renderer.setDebugView(9);
    renderer.setSun(true,false,{-0.45f,-0.35f,-0.82f},1.2f,0.55f,{1,0.98f,0.95f},{0.65f,0.72f,0.82f},1024,500,400,{},false,1024,500,1000,100);

    static constexpr Clip clips[]={{"awp","idle_awp.cast"},{"knife_karambit","idle1_karambit.cast"},{"knife_karambit","lookat01_karambit.cast"},{"knife_karambit","draw_karambit.cast"},{"knife_butterfly","draw_butterfly.cast"},{"knife_talon","draw_talon.cast"}};
    std::ofstream manifest(output/"manifest.csv");manifest<<"clip,frame,view,source_png,target_png\n";std::size_t saved{},expectedPngs{};bool preparedAll=true;
    for(const auto& clip:clips){
        const auto weapon=root/"cs2"/"models"/"weapons"/clip.family/(std::string{clip.family}+"_model.cast");
        const auto animation=root/"cs2"/"animations"/"viewmodel"/clip.family/clip.animation;
        scene::CastScene source,target;std::size_t sourceAnimation{},targetAnimation{};
        if(!loadRig(sourceHandsPath,weapon,animation,1.0f,source,sourceAnimation)||
           !loadRig(targetHandsPath,weapon,animation,targetScale,target,targetAnimation)){
            std::cerr<<"Could not prepare "<<clip.animation<<'\n';preparedAll=false;continue;
        }
        if(saved==0){writeRigWeights(source,output/"cs2_rig_weights.csv");writeRigWeights(target,output/"bo2_rig_weights.csv");}
        if(!scene::retargetSource2AnimationRange(target,targetAnimation,source,sourceAnimation)){std::cerr<<"Could not retarget "<<clip.animation<<'\n';preparedAll=false;continue;}
        const auto duration=std::max<std::uint32_t>(1,source.animations[sourceAnimation].durationFrames);
        std::vector<std::uint32_t> frames{0,duration/2,duration};
        frames.erase(std::unique(frames.begin(),frames.end()),frames.end());
        const std::array<std::pair<const char*,scene::Vec3>,2> views{{{"side",{0,-1,0}},{"top",{0,0,1}}}};
        expectedPngs+=frames.size()*(views.size()+1)*2;
        for(const auto frame:frames){const auto sourcePose=source.samplePose(sourceAnimation,static_cast<float>(frame)),targetPose=target.samplePose(targetAnimation,static_cast<float>(frame));
            const auto sourceBounds=posedBounds(source,sourcePose);if(!sourceBounds.valid)continue;
            for(const auto& [viewName,direction]:views){
                const auto stem=std::filesystem::path{clip.animation}.stem().string()+"_f"+std::to_string(frame)+"_"+viewName;
                const auto sourcePng=output/(stem+"_cs2.png"),targetPng=output/(stem+"_bo2.png");
                const auto sourceCamera=comparisonCamera(sourceBounds,direction,960,720,1.0f),targetCamera=comparisonCamera(sourceBounds,direction,960,720,targetScale);
                if(saveFrame(renderer,source,sourcePose,sourcePng,sourceCamera)&&saveFrame(renderer,target,targetPose,targetPng,targetCamera)){
                    manifest<<clip.animation<<','<<frame<<','<<viewName<<','<<sourcePng.filename().string()<<','<<targetPng.filename().string()<<'\n';saved+=2;
                }
            }
            const auto sourceFirstPerson=firstPersonCamera(1.0f),targetFirstPerson=firstPersonCamera(targetScale);
            const auto stem=std::filesystem::path{clip.animation}.stem().string()+"_f"+std::to_string(frame)+"_firstperson";
            const auto sourcePng=output/(stem+"_cs2.png"),targetPng=output/(stem+"_bo2.png");
            if(saveFrame(renderer,source,sourcePose,sourcePng,sourceFirstPerson)&&saveFrame(renderer,target,targetPose,targetPng,targetFirstPerson)){
                manifest<<clip.animation<<','<<frame<<",firstperson,"<<sourcePng.filename().string()<<','<<targetPng.filename().string()<<'\n';saved+=2;
            }
        }
    }
    renderer.shutdown();glfwDestroyWindow(window);glfwTerminate();
    std::cout<<"VISUAL_REGRESSION_OUTPUT "<<output.string()<<" pngs="<<saved<<" expected="<<expectedPngs<<'\n';return preparedAll&&saved==expectedPngs?0:2;
}
