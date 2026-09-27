#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <iostream>
int main(int argc,char** argv){try{
    if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(960,540,"Viewhand audit",nullptr,nullptr);if(!window)return 2;glfwMakeContextCurrent(window);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
    auto state=std::make_unique<AppState>();auto& app=*state;app.window=window;app.deferSceneUpload=true;std::string error;if(!app.renderer.initialize(error))return 2;
    app.defaultSalukiDirectory="D:/Editing/COD Resource/3D Rip/saluki/exported_files";
    for(const auto game:{"cs2","aw","bo2_sp","ghosts","iw_sp","mw","mw3","mwr","codm","bocw_sp","bo2","pointblank","eldewrito","cs1.6","css","csnz","cso2"})if(!assets::appendScan(app.defaultSalukiDirectory/game,game,app.assetCatalog,error))throw std::runtime_error(error);
    const auto pick=[&](const std::string& game,assets::Role role,const std::string& name){for(size_t i=0;i<app.assetCatalog.entries.size();++i){const auto& a=app.assetCatalog.entries[i];if(a.game==game&&a.role==role&&lowerText(a.name).find(name)!=std::string::npos)return i;}throw std::runtime_error("Missing fixture "+game+" "+name);};
    const std::filesystem::path out=argc>1?argv[1]:"diagnostics/v263/runtime";std::filesystem::create_directories(out);std::ofstream report(out/"results.txt");int failures{};
    const std::vector<std::string> targets=argc>2?std::vector<std::string>{argv[2]}:std::vector<std::string>{"codm","bocw_sp"};
    const std::vector<std::string> sources=argc>3?std::vector<std::string>{argv[3]}:std::vector<std::string>{"cs2","cs1.6","css","csnz","cso2","eldewrito","codm","bocw_sp","aw","bo2","bo2_sp","ghosts","iw_sp","mw","mw3","mwr","pointblank"};
    for(const auto& target:targets)for(const auto& source:sources){
        const auto label=source+"_to_"+target;std::cout<<"CASE "<<label<<std::endl;
        try{
            size_t hand=SIZE_MAX;const std::string wanted=source=="cso2"?"hand_707":source=="eldewrito"?"masterchief_fp_fp":assets::imported::sourceFamily(source)?"hands_shared":source=="codm"?"ghost_1p":"";
            for(size_t i=0;i<app.assetCatalog.entries.size();++i){const auto& a=app.assetCatalog.entries[i];if(a.game==source&&a.role==assets::Role::ViewHands&&lowerText(a.name).find(wanted)!=std::string::npos&&scene::imported::viewLayout(scene::buildScene(cast::Document::load(a.path))).usable()){hand=i;break;}}
            if(hand==SIZE_MAX)throw std::runtime_error("Missing usable hand fixture");
            const auto weapon=pick(target,assets::Role::ViewWeapon,argc>4?argv[4]:target=="codm"?"viewmodel_ar_ak47":"wpn_t9_sniper_standard_view_lod0");
            app.selectedBaseAsset=hand;app.selectedWeaponAsset=weapon;equipViewWeapon(app,weapon);
            if(app.scene.animations.empty()||app.loadedRigModelPaths.empty())throw std::runtime_error(app.status);
            const auto live=app.scene;captureTakeActorManifest(app);const auto manifest=app.recordedTake.actor;
            const auto clip=std::min(app.animationIndex,live.animations.size()-1);const auto pose=live.samplePose(clip,0);
            app.renderer.loadScene(live,error);app.renderer.setDebugView(1);app.renderer.setViewmodelCapture(true,{.08f,.09f,.11f,1});
            scene::Vec3 eye{},forward{1,0,0},up{0,0,1};if(const auto it=live.skeleton.boneByCanonicalName.find("tag_camera");it!=live.skeleton.boneByCanonicalName.end()){const auto& c=pose[it->second];eye={c.v[12],c.v[13],c.v[14]};forward={c.v[0],c.v[1],c.v[2]};up={c.v[8],c.v[9],c.v[10]};}
            const auto vp=scene::perspective(75*scene::kPi/180,960.f/540,.02f,5000)*scene::lookAtDirection(eye,forward,up);app.renderer.setCameraPosition(eye);app.renderer.render(live,pose,vp,960,540,false,false,false);app.renderer.saveColorPng(out/(label+".png"),error);
            for(size_t c=0;c<live.animations.size();++c)for(float t:{0.f,.5f,1.f})for(const auto& m:live.samplePose(c,live.animations[c].durationFrames*t))for(float x:m.v)if(!std::isfinite(x))throw std::runtime_error("Nonfinite pose");
            app.scene={};app.classSlotRigs={};app.selectedWeaponAsset=SIZE_MAX;
            if(!restoreTakeActor(app,manifest,live.skeleton.bones.size(),error))throw std::runtime_error("Replay: "+error);
            if(app.scene.meshes.size()!=live.meshes.size()||app.scene.skeleton.bones.size()!=live.skeleton.bones.size())throw std::runtime_error("Replay structure mismatch");
            for(size_t m=0;m<live.meshes.size();++m){const auto& a=live.meshes[m];const auto& b=app.scene.meshes[m];if(a.vertices.size()!=b.vertices.size())throw std::runtime_error("Replay vertex count mismatch");for(size_t v=0;v<a.vertices.size();++v)if(scene::length(a.vertices[v].position-b.vertices[v].position)>.002f)throw std::runtime_error("Replay skin mismatch");}
            auto catalog=std::move(app.assetCatalog);const bool disabledOk=restoreTakeActor(app,manifest,live.skeleton.bones.size(),error);app.assetCatalog=std::move(catalog);
            if(!disabledOk)throw std::runtime_error("Disabled-library replay: "+error);
            if(app.scene.meshes.size()!=live.meshes.size())throw std::runtime_error("Disabled-library replay mesh count");
            report<<"PASS "<<label<<" clips="<<live.animations.size()<<" bones="<<live.skeleton.bones.size()<<" disabled-library-replay=pass"<<std::endl;
        }catch(const std::exception& e){report<<"FAIL "<<label<<" "<<e.what()<<std::endl;std::cerr<<e.what()<<std::endl;++failures;}
    }
    std::cout<<"Failures "<<failures<<std::endl;app.renderer.shutdown();glfwDestroyWindow(window);glfwTerminate();return failures?1:0;
}catch(const std::exception& e){std::cerr<<e.what();return 2;}}
