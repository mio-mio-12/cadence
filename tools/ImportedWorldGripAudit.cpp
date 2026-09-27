#include "scene/ImportedWorldGrip.h"
#include "scene/ImportedBodyRetarget.h"
#include "render/StageRenderer.h"
#include "app/WorldWeaponAssembly.h"
#include <GLFW/glfw3.h>
#include <iostream>
int audit(int argc,char** argv){
    if((argc==3||argc==4)&&std::string_view(argv[1])=="--measure"){
        auto s=scene::buildScene(cast::Document::load(argv[2]),false);
        if(argc==4){
            scene::appendAnimations(cast::Document::load(argv[3]),s);
            const auto p=s.samplePose(0,0);
            for(const auto& arm:scene::imported::viewLayout(s).arms)if(arm.wrist>=0){
                const auto v=scene::transformPoint(p[arm.wrist],{});
                std::cout<<s.skeleton.bones[arm.wrist].name<<" wrist "<<v.x<<','<<v.y<<','<<v.z<<'\n';
            }
        }
        scene::Vec3 lo{1e10f,1e10f,1e10f},hi{-1e10f,-1e10f,-1e10f};
        for(const auto& mesh:s.meshes)for(const auto& v:mesh.vertices){
            const auto p=scene::transformPoint(mesh.modelTransform,v.position);
            lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};
            hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};
        }
        std::cout<<"Bounds "<<hi.x-lo.x<<','<<hi.y-lo.y<<','<<hi.z-lo.z<<" translationScale "<<s.importedTranslationScale<<'\n';
        for(const auto& b:s.skeleton.bones)if(b.name.find("wrist")!=std::string::npos||b.name.find("Hand")!=std::string::npos){
            scene::Vec3 p,k;scene::Quat q;scene::decomposeAffine(b.restGlobal,p,q,k);
            std::cout<<b.name<<" scale "<<k.x<<','<<k.y<<','<<k.z<<'\n';
        }
        return 0;
    }
    if(argc==4&&std::string_view(argv[1])=="--pose"){
        auto actor=scene::buildScene(cast::Document::load(argv[2]),false);
        scene::appendAnimations(cast::Document::load(argv[3]),actor);
        const auto pose=actor.samplePose(actor.animations.size()-1,0);
        for(const auto& a:actor.animations)std::cout<<a.name<<" tracks="<<a.tracks.size()<<" missing="<<a.unmappedCurveCount<<'\n';
        for(std::size_t i=0;i<actor.skeleton.bones.size();++i){const auto& b=actor.skeleton.bones[i];if(b.name.find("Hand")!=std::string::npos||b.name.find("W_bone")!=std::string::npos){
            std::cout<<b.name<<" rest=";for(auto j:{12,13,14})std::cout<<b.restGlobal.v[j]<<',';
            std::cout<<" pose=";for(auto j:{12,13,14})std::cout<<pose[i].v[j]<<',';
            std::cout<<" matrix=";for(auto v:pose[i].v)std::cout<<v<<',';std::cout<<'\n';}}
        return 0;
    }
    // donor body, donor hold, raw world weapon, game, target body, target hold, output
    if(argc!=8&&argc!=9)return 2;
    for(const int i:{1,2,3,5,6})if(std::string_view(argv[i])!="-"&&!cast::Document::load(argv[i]).valid()){std::cerr<<"Invalid/missing audit input: "<<argv[i]<<'\n';return 2;}
    std::cerr<<"Loading donor\n";auto donor=scene::buildScene(cast::Document::load(argv[1]),false);
    if(std::string_view(argv[2])!="-")scene::appendAnimations(cast::Document::load(argv[2]),donor);
    std::cerr<<"Sampling donor\n";const auto sourcePose=donor.samplePose(0,0);
    std::cerr<<"Loading weapon\n";const auto worldDoc=cast::Document::load(argv[3]);auto world=scene::buildScene(worldDoc,false);
    std::cerr<<"Loading target\n";auto actor=scene::buildScene(cast::Document::load(argv[5]),false);
    if(std::string_view(argv[6])!="-"){
        if(argc==9){auto motion=std::make_shared<scene::CastScene>(scene::buildScene(cast::Document::load(argv[8]),false));scene::appendAnimations(cast::Document::load(argv[6]),*motion);std::string bindError;if(!scene::imported::appendRetargetedBody(actor,motion,0,bindError)){std::cerr<<bindError;return 1;}}
        else if(std::string_view(argv[6])==argv[2]&&scene::imported::bodyLayout(actor.skeleton)&&std::string_view(argv[4])=="bo2"){
            std::string bindError;if(!scene::imported::appendRetargetedBody(actor,std::make_shared<scene::CastScene>(donor),0,bindError)){std::cerr<<bindError;return 1;}
        }else scene::appendAnimations(cast::Document::load(argv[6]),actor);
    }
    const bool viewFallback=std::string_view(argv[4]).starts_with("view");
    if(viewFallback&&std::string_view(argv[2])!="-")scene::appendAnimations(cast::Document::load(argv[2]),world);
    if(viewFallback){
        const auto layout=scene::imported::viewLayout(world);
        const auto p=world.samplePose(0,0);
        for(const auto& arm:layout.arms)if(arm.wrist>=0){
            const auto v=scene::transformPoint(p[arm.wrist],{});
            std::cout<<world.skeleton.bones[arm.wrist].name<<" wrist "<<v.x<<','<<v.y<<','<<v.z<<'\n';
        }
        const auto p2=actor.samplePose(0,0);
        for(const auto& name:{"j_wrist_le","j_wrist_ri"})if(auto i=actor.skeleton.boneByName.find(name);i!=actor.skeleton.boneByName.end()){
            const auto v=scene::transformPoint(p2[i->second],{});
            std::cout<<name<<" wrist "<<v.x<<','<<v.y<<','<<v.z<<'\n';
        }
    }
std::cerr<<"Calculating grip\n";std::string error;const auto m=viewFallback?scene::imported::world_grip::fromViewHold(world,world.animations.empty()?scene::imported::world_grip::restPose(world.skeleton):world.samplePose(0,0),actor.skeleton,2.54f,std::string_view(argv[4])=="viewpalm"?nullptr:&actor.skeleton,actor.samplePose(0,0),true):scene::imported::world_grip::fromNativeSocket(argv[4],world,donor.skeleton,sourcePose,actor.skeleton,error);
    if(!m){std::cerr<<error<<'\n';return 1;}
    if(viewFallback){
        scene::Vec3 lo{1e10f,1e10f,1e10f},hi{-1e10f,-1e10f,-1e10f};
        for(const auto& mesh:world.meshes)for(const auto& v:mesh.vertices){
            const auto p=scene::transformPoint(m->transform*mesh.modelTransform,v.position);
            lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};
            hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};
        }
        std::cout<<"Frozen mounted dimensions "<<hi.x-lo.x<<','<<hi.y-lo.y<<','<<hi.z-lo.z<<'\n';
    }
    const auto before=actor.attachments.size();if(viewFallback)scene::appendPreparedAttachment(std::move(world),actor,m->bone,"world grip audit");else scene::appendAttachment(worldDoc,actor,m->bone,"world grip audit");
    if(actor.attachments.size()==before)return 2;
    cadence::world_weapon::setTransform(actor.attachments.back(),m->transform);
    std::cout<<"Mounted on "<<actor.skeleton.bones[m->bone].name<<" scale="<<actor.attachments.back().scale.x<<','<<actor.attachments.back().scale.y<<','<<actor.attachments.back().scale.z<<'\n';
    if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* w=glfwCreateWindow(1000,750,"Grip audit",nullptr,nullptr);if(!w)return 2;glfwMakeContextCurrent(w);
    render::StageRenderer r;if(!r.initialize(error)||!r.loadScene(actor,error)){std::cerr<<error;return 2;}
    r.setDebugView(1);r.setViewmodelCapture(true,{.08f,.09f,.11f,1});
    const auto pose=actor.samplePose(0,0);const auto wrist=scene::transformPoint(pose[m->bone],{});
    const auto folder=std::filesystem::path(argv[7]);std::filesystem::create_directories(folder);
    for(int i=0;i<3;++i){const auto center=wrist+scene::Vec3{12,0,0};const auto eye=center+(i==0?scene::Vec3{100,-140,65}:i==1?scene::Vec3{100,140,65}:scene::Vec3{20,-65,30});
        const auto vp=scene::perspective(48*scene::kPi/180,1000.f/750.f,.1f,2000)*scene::lookAt(eye,center,{0,0,1});
        r.render(actor,pose,vp,1000,750,false,false,false);
        if(const auto muzzle=scene::resolveMuzzlePosition(actor,pose)){
            r.renderMuzzleFlash3D(*muzzle,4.f,0,{1,.6f,.1f,1},vp,eye,false);
            r.renderDebugLine3D(*muzzle-scene::Vec3{0,0,4},*muzzle+scene::Vec3{0,0,4},{0,1,0,1},vp);
            std::cout<<"World muzzle "<<muzzle->x<<','<<muzzle->y<<','<<muzzle->z<<'\n';
        }else{std::cerr<<"World muzzle missing\n";return 3;}
        if(!r.saveColorPng(folder/("grip"+std::to_string(i)+".png"),error)){std::cerr<<error;return 2;}}
    r.shutdown();glfwDestroyWindow(w);glfwTerminate();return 0;
}
int main(int argc,char** argv){try{return audit(argc,argv);}catch(const std::exception& e){std::cerr<<"AUDIT ERROR: "<<e.what()<<'\n';return 1;}}
