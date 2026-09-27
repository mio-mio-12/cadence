#include "scene/ImportedNative.h"
#include "render/StageRenderer.h"
#include <GLFW/glfw3.h>
#include <iostream>
int runAudit(int argc,char** argv){
    if(argc<5)return 2;
    std::cerr<<"[native audit] Loading weapon document\n";
    const auto weapon=cast::Document::load(argv[1]);scene::CastScene s;std::string error;
    if(!weapon.valid()){std::cerr<<"Invalid weapon document: "<<argv[1]<<'\n';return 2;}
    std::cerr<<"[native audit] Building scene\n";
    if(std::string_view(argv[2])!="-"){
        const auto hands=cast::Document::load(argv[2]);if(!hands.valid()){std::cerr<<"Invalid hands document: "<<argv[2]<<'\n';return 2;}
        if(!scene::imported::assemble(weapon,hands,s,error)){std::cerr<<error;return 2;}
        if(std::getenv("CADENCE_NATIVE_BIND_AUDIT")){
            const auto skin=scene::buildScene(hands,false),gun=scene::buildScene(weapon,false);
            for(const auto& b:skin.skeleton.bones){const auto n=assets::imported::lower(b.name);if(n.find("arm")==std::string::npos&&n.find("wrist")==std::string::npos&&n.find("hand")==std::string::npos)continue;
                const auto it=gun.skeleton.boneByName.find(b.name);float delta{};if(it!=gun.skeleton.boneByName.end())for(int k=0;k<16;++k)delta=std::max(delta,std::abs(b.restGlobal.v[k]-gun.skeleton.bones[it->second].restGlobal.v[k]));
                std::cout<<"BIND "<<b.name<<" parent="<<b.parent<<" in_weapon="<<(it!=gun.skeleton.boneByName.end())<<" delta="<<delta<<'\n';
            }
            if(std::string_view(std::getenv("CADENCE_NATIVE_BIND_AUDIT"))=="skin")s=skin;
        }
    }else s=scene::buildScene(weapon,false);
    if(std::string_view(argv[3])!="-"){
        std::cerr<<"[native audit] Binding animation\n";
        const auto clip=cast::Document::load(argv[3]);if(!clip.valid()){std::cerr<<"Invalid animation document: "<<argv[3]<<'\n';return 2;}
        if(!scene::appendAnimations(clip,s)){std::cerr<<"No animation appended from: "<<argv[3]<<'\n';return 2;}
        if(std::getenv("CADENCE_NATIVE_BIND_AUDIT"))for(std::size_t i=0;i<s.skeleton.bones.size();++i){const auto& b=s.skeleton.bones[i];const auto n=assets::imported::lower(b.name);if(n.find("arm")==std::string::npos&&n.find("wrist")==std::string::npos&&n.find("hand")==std::string::npos)continue;std::size_t tracks{};for(const auto& t:s.animations.back().tracks)tracks+=t.boneIndex==i;std::cout<<"TRACKS "<<b.name<<" count="<<tracks<<'\n';}
    }
    std::cerr<<"[native audit] Initializing GL\n";
    if(std::getenv("CADENCE_NATIVE_BIND_AUDIT")){
        const auto local=s.sampleLocalPose(0,0);for(std::size_t i=0;i<s.skeleton.bones.size();++i){const auto& b=s.skeleton.bones[i];if(b.name!="l_hand"&&b.name!="l_forearm"&&b.name!="r_hand"&&b.name!="r_forearm")continue;const auto chain=(b.parent>=0?s.skeleton.bones[b.parent].restGlobal:scene::Mat4::identity())*scene::trs(b.restLocal.position,b.restLocal.rotation,b.restLocal.scale);float mismatch{};for(int k=0;k<16;++k)mismatch=std::max(mismatch,std::abs(chain.v[k]-b.restGlobal.v[k]));const auto p=b.restLocal.position,q=local[i].position;const auto r=b.restLocal.rotation,t=local[i].rotation;std::cout<<"POSE "<<b.name<<" chain_error="<<mismatch<<" rest="<<p.x<<','<<p.y<<','<<p.z<<" animated="<<q.x<<','<<q.y<<','<<q.z<<" restq="<<r.x<<','<<r.y<<','<<r.z<<','<<r.w<<" q="<<t.x<<','<<t.y<<','<<t.z<<','<<t.w<<'\n';}
    }
    if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    auto* window=glfwCreateWindow(960,540,"Native import visual audit",nullptr,nullptr);if(!window)return 2;glfwMakeContextCurrent(window);
    std::cerr<<"[native audit] Initializing renderer\n";
    render::StageRenderer renderer;if(!renderer.initialize(error)){std::cerr<<error;return 2;}
    if(std::getenv("CADENCE_AUDIT_UNCACHED"))renderer.setPoseUploadCacheEnabled(false);
    std::cerr<<"[native audit] Uploading scene\n";
    if(!renderer.loadScene(s,error)){std::cerr<<error;return 2;}
    renderer.setDebugView(1);renderer.setViewmodelCapture(true,{.08f,.09f,.11f,1});
    const std::filesystem::path output=argv[4];std::filesystem::create_directories(output);
    for(const auto frame:{0.f,.5f}){
        std::cerr<<"[native audit] Rendering frame fraction "<<frame<<'\n';
        auto pose=s.samplePose(0,s.animations.empty()?0:s.animations[0].durationFrames*frame);
        if(const auto* mode=std::getenv("CADENCE_NATIVE_BIND_AUDIT");mode&&std::string_view(mode)=="twist"){
            auto local=s.sampleLocalPose(0,s.animations.empty()?0:s.animations[0].durationFrames*frame);
            for(const auto* side:{"l_","r_"}){const auto hand=s.skeleton.boneByName.find(std::string(side)+"hand"),forearm=s.skeleton.boneByName.find(std::string(side)+"forearm");if(hand==s.skeleton.boneByName.end()||forearm==s.skeleton.boneByName.end())continue;
const auto h=hand->second,f=forearm->second;const auto q=local[h].rotation;const float amount=std::getenv("CADENCE_FOREARM_ROLL")?std::strtof(std::getenv("CADENCE_FOREARM_ROLL"),nullptr):.5f;const auto half=scene::slerp(scene::Quat{},scene::normalize(scene::Quat{q.x,0,0,q.w}),amount);const scene::Quat inv{-half.x,-half.y,-half.z,half.w};local[f].rotation=scene::multiply(local[f].rotation,half);local[h].rotation=scene::multiply(inv,local[h].rotation);local[h].position=scene::transformPoint(scene::trs({},inv,{1,1,1}),local[h].position);
            }
            pose=s.globalPose(local);
        }
        auto draw=s;
        if(const auto* mode=std::getenv("CADENCE_NATIVE_BIND_AUDIT");mode&&std::string_view(mode)=="dq"){
            for(auto& mesh:draw.meshes)if(mesh.skinned){for(auto& v:mesh.vertices){scene::Quat real{0,0,0,0},dual{0,0,0,0},reference{};bool first=true;
                for(int k=0;k<4;++k)if(v.weights[k]>0&&v.bones[k]<pose.size()){
                    const auto matrix=pose[v.bones[k]]*s.skeleton.bones[v.bones[k]].inverseBind;scene::Vec3 p,scale;scene::Quat q;scene::decomposeAffine(matrix,p,q,scale);auto d=scene::multiply(scene::Quat{p.x,p.y,p.z,0},q);if(first){reference=q;first=false;}const float sign=q.x*reference.x+q.y*reference.y+q.z*reference.z+q.w*reference.w<0?-1.f:1.f,w=v.weights[k]*sign;
                    real={real.x+q.x*w,real.y+q.y*w,real.z+q.z*w,real.w+q.w*w};dual={dual.x+d.x*w*.5f,dual.y+d.y*w*.5f,dual.z+d.z*w*.5f,dual.w+d.w*w*.5f};
                }
                const float norm=std::sqrt(real.x*real.x+real.y*real.y+real.z*real.z+real.w*real.w);if(norm>1e-7f){real={real.x/norm,real.y/norm,real.z/norm,real.w/norm};dual={dual.x/norm,dual.y/norm,dual.z/norm,dual.w/norm};const auto t=scene::multiply(dual,scene::Quat{-real.x,-real.y,-real.z,real.w});const auto transform=scene::trs({t.x*2,t.y*2,t.z*2},real,{1,1,1});v.position=scene::transformPoint(transform,v.position);v.normal=scene::normalize(scene::transformPoint(transform,v.normal)-scene::transformPoint(transform,{}));}
            }mesh.skinned=false;}if(!renderer.loadScene(draw,error))return 2;
        }
        for(const auto axis:{0,1,2,3}){
            const scene::Vec3 forward=axis==0?scene::Vec3{1,0,0}:axis==1?scene::Vec3{0,-1,0}:axis==2?scene::Vec3{-1,0,0}:scene::Vec3{0,1,0};
            const auto vp=scene::perspective(75*scene::kPi/180,960.f/540.f,.02f,5000)*scene::lookAt({},forward,{0,0,1});
            renderer.render(draw,pose,vp,960,540,false,false,false);
            if(!renderer.saveColorPng(output/("axis"+std::to_string(axis)+"_frame"+std::to_string(frame)+".png"),error)){std::cerr<<error;return 2;}
        }
    }
    renderer.shutdown();glfwDestroyWindow(window);glfwTerminate();return 0;
}
int main(int argc,char** argv){
    try{return runAudit(argc,argv);}
    catch(const std::exception& error){std::cerr<<"Native visual audit exception: "<<error.what()<<'\n';return 1;}
    catch(...){std::cerr<<"Native visual audit unknown exception\n";return 1;}
}
