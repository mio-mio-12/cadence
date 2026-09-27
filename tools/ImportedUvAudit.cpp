#include "scene/CastScene.h"
#include "scene/ImportedNative.h"
#include "assets/ImportedGamePolicy.h"
#include "render/StageRenderer.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <functional>
#include <limits>

// Diagnostic only: retains source files and compares orientation hypotheses
// in separate off-screen renders, never changes production UVs.
int main(int argc,char** argv){
    if(argc<3)return 2;
    const auto doc=cast::Document::load(std::filesystem::u8path(argv[1]));
    if(!doc.valid())return 2;
    auto original=scene::buildScene(doc,false);
    const bool normalizeV=scene::imported::dewExport(doc);
    std::cout<<"UV_CONVENTION "<<(normalizeV?"dew2cast: expected (u,1-v)":"unchanged (u,v)")<<'\n';
    std::vector<const cast::Node*> meshes;
    std::function<void(const cast::Node&)> visit=[&](const cast::Node& n){if(n.findProperty("u0"))meshes.push_back(&n);if(cast::nodeTypeName(n.identifier)=="Material")for(const auto& p:n.properties)if(p.stringValue)std::cout<<"RAW_MATERIAL "<<p.name<<'='<<*p.stringValue<<'\n';for(const auto& c:n.children)visit(c);};
    for(const auto& r:doc.roots())visit(r);
    std::size_t mismatches{},missingUv{},checked{};
    for(const auto& m:original.meshes){
        const cast::Node* raw=nullptr;
        for(auto* n:meshes){const auto* p=n->findProperty("n");if(p&&p->stringValue&&*p->stringValue==m.name){raw=n;break;}}
        if(!raw){++missingUv;std::cout<<"MISSING_RAW_UV "<<m.name<<'\n';continue;}
        const auto uv=doc.floatValues(*raw->findProperty("u0"));
        if(uv.size()!=m.vertices.size()*2){++missingUv;continue;}
        for(std::size_t i=0;i<m.vertices.size();++i){++checked;const auto expectedV=normalizeV?1.f-uv[i*2+1]:uv[i*2+1];mismatches+=m.vertices[i].uv.x!=uv[i*2]||m.vertices[i].uv.y!=expectedV;}
        std::cout<<"MATERIAL "<<m.name<<" => "<<assets::imported::utf8(m.albedoPath)<<'\n';
    }
    std::cout<<"UV checked="<<checked<<" mismatches="<<mismatches<<" missing="<<missingUv<<'\n';
    if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    auto* window=glfwCreateWindow(1000,750,"UV audit",nullptr,nullptr);if(!window)return 2;glfwMakeContextCurrent(window);
    render::StageRenderer renderer;std::string error;if(!renderer.initialize(error)){std::cerr<<error;return 2;}
    renderer.setDebugView(1);renderer.setViewmodelCapture(true,{.08f,.09f,.11f,1});
    scene::Vec3 lo{1e9f,1e9f,1e9f},hi{-1e9f,-1e9f,-1e9f};
    for(const auto& m:original.meshes)for(const auto& v:m.vertices){lo.x=std::min(lo.x,v.position.x);lo.y=std::min(lo.y,v.position.y);lo.z=std::min(lo.z,v.position.z);hi.x=std::max(hi.x,v.position.x);hi.y=std::max(hi.y,v.position.y);hi.z=std::max(hi.z,v.position.z);}
    const auto center=(lo+hi)*.5f;const auto radius=std::max({hi.x-lo.x,hi.y-lo.y,hi.z-lo.z})*.8f;
    const std::filesystem::path out=std::filesystem::u8path(argv[2]);std::filesystem::create_directories(out);
    for(int variant=0;variant<4;++variant){
        auto s=original;for(auto& m:s.meshes)for(auto& v:m.vertices){if(variant&1)v.uv.y=1-v.uv.y;if(variant&2)v.uv.x=1-v.uv.x;}
        if(!renderer.loadScene(s,error)){std::cerr<<error;return 2;}
        for(int angle=0;angle<2;++angle){
            const auto eye=center+scene::Vec3{radius*(angle?-1.4f:1.4f),radius*-1.4f,radius*.7f};
            const auto vp=scene::perspective(45*scene::kPi/180,1000.f/750.f,std::max(.001f,radius*.001f),radius*20)*scene::lookAt(eye,center,{0,0,1});
            renderer.render(s,s.samplePose(0,0),vp,1000,750,false,false,false);
            if(!renderer.saveColorPng(out/("uv"+std::to_string(variant)+"_side"+std::to_string(angle)+".png"),error)){std::cerr<<error;return 2;}
        }
    }
    renderer.shutdown();glfwDestroyWindow(window);glfwTerminate();return mismatches||missingUv?1:0;
}
