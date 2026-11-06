#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main
#include <iostream>
int main(int argc,char** argv){
    try{
        if(argc!=2||!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(960,540,"BO foreign hands audit",nullptr,nullptr);if(!window)return 2;glfwMakeContextCurrent(window);
        ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
        const auto root=std::filesystem::path("exported_files");const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);std::ofstream log(out/"results.txt");
        assets::Catalog catalog;std::string error;
        for(const char* game:{"bo","bo2","cs2","cs1.6","css","csnz","cso2"})if(!assets::appendScan(root/game,game,catalog,error))throw std::runtime_error(error);
        int failures{};
        for(const char* game:{"bo2","cs2","cs1.6","css","csnz","cso2"}){
            size_t hand=SIZE_MAX;int rank=-1;
            for(size_t i=0;i<catalog.entries.size();++i){const auto& a=catalog.entries[i];if(a.role!=assets::Role::ViewHands||a.game!=game)continue;const auto name=lowerText(a.name);const int score=name=="viewhands_ctm_fbi_hands_model"||name=="hands_shared"||name=="hand_707"||name.starts_with("c_usa_mp_seal6_longsleeve_viewhands")?100:1;if(score>rank){rank=score;hand=i;}}
            if(hand==SIZE_MAX){log<<"FAIL no hands "<<game<<'\n';++failures;continue;}
            for(const char* name:{"t5_weapon_1911_viewmodel_LOD0","t5_weapon_ak47_viewmodel_LOD0","t6_wpn_ar_an94_view_LOD0"}){
                auto state=std::make_unique<AppState>();auto& app=*state;app.window=window;app.assetCatalog=catalog;app.defaultSalukiDirectory=root;app.deferSceneUpload=true;app.selectedBaseAsset=hand;
                // Exercise reference lookup with BO2 disabled, without changing
                // the real catalog or user game selection.
                if(std::string(game)=="cs2"&&std::string(name)=="t5_weapon_1911_viewmodel_LOD0"){
                    const auto path=catalog.entries[hand].path;std::erase_if(app.assetCatalog.entries,[](const auto& a){return a.game=="bo2";});
                    for(size_t i=0;i<app.assetCatalog.entries.size();++i)if(app.assetCatalog.entries[i].path==path)app.selectedBaseAsset=i;
                    log<<"BO2 reference library disabled for CS2/1911 test\n";
                }
                size_t weapon=SIZE_MAX;for(size_t i=0;i<app.assetCatalog.entries.size();++i)if((app.assetCatalog.entries[i].game=="bo"||app.assetCatalog.entries[i].game=="bo2")&&app.assetCatalog.entries[i].name==name){weapon=i;break;}
                if(weapon==SIZE_MAX){log<<"FAIL no gun "<<name<<'\n';++failures;continue;}
                equipViewWeapon(app,weapon);const auto label=std::string(game)+"_"+name;
                log<<label<<" bones="<<app.scene.skeleton.bones.size()<<" clips="<<app.scene.animations.size()<<" driver="<<app.scene.viewHandsDriverGame<<" status="<<app.status<<'\n';
                for(const auto& warning:app.scene.warnings)log<<"WARNING "<<warning<<'\n';
                const auto idle=findViewmodelClip(app,"idle"),reload=findViewmodelClip(app,"reload"),pullout=findViewmodelClip(app,"pullout");
                if(!idle||!reload||!pullout){log<<"FAIL required actions\n";++failures;continue;}
                if(!app.renderer.initialize(error)||!app.renderer.loadScene(app.scene,error))throw std::runtime_error(error);app.renderer.setDebugView(1);app.renderer.setViewmodelCapture(true,{.08f,.09f,.11f,1});
                for(const auto action:std::array<std::pair<const char*,size_t>,3>{{{"idle",*idle},{"reload",*reload},{"pullout",*pullout}}})for(float fraction:{0.f,.5f,1.f}){
                    const auto pose=app.scene.samplePose(action.second,app.scene.animations[action.second].durationFrames*fraction);bool finite=true;for(const auto& m:pose)for(float v:m.v)finite&=std::isfinite(v);if(!finite){log<<"FAIL nonfinite pose\n";++failures;continue;}
                    const auto camera=app.scene.skeleton.boneByCanonicalName.find("tag_camera");if(camera==app.scene.skeleton.boneByCanonicalName.end()){log<<"FAIL camera\n";++failures;continue;}
                    const auto c=pose[camera->second];const scene::Vec3 eye{c.v[12],c.v[13],c.v[14]},forward{c.v[0],c.v[1],c.v[2]},up{c.v[8],c.v[9],c.v[10]};
                    app.renderer.setCameraPosition(eye);app.renderer.render(app.scene,pose,scene::perspective(75*scene::kPi/180,960.f/540,.1f,2000)*scene::lookAtDirection(eye,forward,up),960,540,false,false,false);
                    if((fraction==.5f||std::string(action.first)=="idle")&&!app.renderer.saveColorPng(out/(label+"_"+action.first+"_"+std::to_string(fraction)+".png"),error))throw std::runtime_error(error);
                }
                log<<"PASS actions and finite poses "<<label<<'\n';log.flush();std::cout<<"Checked "<<label<<'\n';
            }
        }
        ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();return failures?1:0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
