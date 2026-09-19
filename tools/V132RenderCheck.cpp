#include "render/StageRenderer.h"
#include "../tests/ReferencePoseV131.h"
#include <cstring>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<"\n";std::exit(1);}}while(false)
int main(int argc,char** argv){
    CHECK(argc>1);const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
    CHECK(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(640,360,"v132 replay render regression",nullptr,nullptr);CHECK(window);glfwMakeContextCurrent(window);
    render::StageRenderer renderer;std::string error;CHECK(renderer.initialize(error));
    auto rig=scene::buildScene(cast::Document::load("D:/Editing/COD Resource/3D Rip/saluki/exported_files/bo2/models/playermodels/isa/c_usa_mp_isa_assault_fb/c_usa_mp_isa_assault_fb_LOD0.cast"),false);CHECK(!rig.meshes.empty());CHECK(!rig.skeleton.bones.empty());
    const std::filesystem::path root="D:/Editing/COD Resource/3D Rip/saluki/exported_files";
    for(const auto& path:{"bo2/animations/pb/combatrun/generic/pb_combatrun_forward_loop.cast","bo2/animations/pt/reload/rifle/pt_rifle_stand_reload_rearclip.cast","bocw_sp/animations/pb/run/assault rifle/pb_rifle_run_slide_land_l.cast"}){
        auto clip=cast::Document::load(root/path);CHECK(clip.valid());scene::appendAnimations(clip,rig);
    }
    CHECK(rig.animations.size()>=3);scene::ReferenceSceneV131 reference;static_cast<scene::CastScene&>(reference)=rig;
    scene::CastScene map;
    for(int i=0;i<15;++i){
        scene::Mesh m;m.materialPolicyExplicit=true;m.doubleSided=true;m.unlit=true;m.forceAlpha=true;m.renderQueue=3000+i%3;
        const float x=float(i%5)*25-50,y=float(i/5)*28;
        m.color={float(i%3==0),float(i%3==1),float(i%3==2),.4f};
        m.vertices={{{x-20,y,20}},{{x+20,y,20}},{{x,y,150}}};m.indices={0,1,2};
        if(i%4==0){m.decalAdditive=true;m.decal=true;}map.meshes.push_back(m);
    }
    CHECK(renderer.loadScene(rig,error));CHECK(renderer.loadAuxiliaryScenes(&map,&rig,&rig,error));
    renderer.setSun(true,true,{.4f,.3f,-1},1,.4f,{1,1,1},{1,1,1},512,800,600,{},true,512,600,1600,100);
    renderer.setViewmodelShadows(true,512);
    std::vector<std::vector<scene::Mat4>> bots(5);std::vector<int> variants(5,-1);
    std::vector<std::uint8_t> baseline,optimized;
    const int frames[]={0,1,2,8,3,15,4,0,20,2,25,0};
    int compared=0;
    for(const int frame:frames){
        // Replay-like backward seeks, same-size pose replacement, changing hidden
        // bones, five actors, and map re-upload/weapon-sized scene replacement.
        rig.hiddenBones.clear();if(frame%3==0)rig.hiddenBones.insert(rig.skeleton.bones.size()/2);if(frame%7==0)rig.hiddenBones.insert(0);
        if(frame==15){auto replacement=rig;CHECK(renderer.replaceMainScene(replacement,&rig,&rig,error));}
        scene::PoseLayer layer;layer.animation=frame%2?1:2;layer.frame=float(frame)+.25f;layer.weight=.7f;layer.mode=scene::LayerMode::Override;
        const std::vector<scene::PoseSlot> slots={{{layer}}};
        auto pose=rig.samplePoseSlots(0,float(frame)+.33f,slots),oldPose=reference.samplePoseSlots(0,float(frame)+.33f,slots);
        CHECK(pose.size()==oldPose.size());CHECK(std::memcmp(pose.data(),oldPose.data(),pose.size()*sizeof(scene::Mat4))==0);
        for(std::size_t i=0;i<pose.size();++i){pose[i].v[12]+=float(frame)*.3f;pose[i].v[14]+=float(frame%3);}
        for(int b=0;b<5;++b){bots[b]=pose;for(auto& m:bots[b]){m.v[12]+=float(b-2)*55;m.v[13]+=100;}}
        scene::Vec3 camera{260+float(frame),-350,190};renderer.setCameraPosition(camera);
        const auto vp=scene::perspective(55*scene::kPi/180,640.f/360,1.f,2000.f)*scene::lookAt(camera,{0,60,65},{0,0,1});
        render::ActorOverlaySettings settings;settings.transparentWorld=frame%2;renderer.setActorOverlays(settings,frame/30.,{}, {},true);
        for(const bool enabled:{false,true}){
            renderer.setPoseUploadCacheEnabled(enabled);renderer.setSortPreparationEnabled(enabled);
            renderer.render(rig,pose,vp,640,360,false,false,false,&rig,&bots,&variants,&rig,&pose,true,&map);
            CHECK(renderer.readColorRgba(enabled?optimized:baseline,error));
            if(enabled&&frame==2)CHECK(renderer.saveColorPng(out/"five_actor_visibility_transparency.png",error));
        }
        CHECK(baseline==optimized);++compared;
    }
    CHECK(renderer.renderStats(false).poseUploads>0);
    std::cout<<"PASS "<<compared<<" byte-identical rendered comparisons: five BO2 actors, shadows, transparent/additive surfaces, backward seeks, hide/show, scene replacement\n";
    renderer.shutdown();glfwDestroyWindow(window);glfwTerminate();
}
