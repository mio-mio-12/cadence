#include "assets/LocalAssetPaths.h"
#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main
int main(){
    if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(960,540,"Import audit",nullptr,nullptr);if(!window)return 2;glfwMakeContextCurrent(window);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
    auto state=std::make_unique<AppState>();auto& a=*state;a.window=window;a.deferSceneUpload=true;std::string error;if(!a.renderer.initialize(error))return 2;
    a.defaultSalukiDirectory=cadence::local_assets::exportPath("");int failures{};
    std::filesystem::create_directories("diagnostics/imported_v229");
    for(const auto* game:{"eldewrito","cs1.6","cz","css","csnz","cso2"}){
        if(const auto* only=std::getenv("CADENCE_AUDIT_GAME");only&&std::string_view(only)!=game)continue;
        a.assetCatalog.clear();if(!assets::appendScan(a.defaultSalukiDirectory/game,game,a.assetCatalog,error)){std::cerr<<error;return 2;}
        std::size_t hands=a.assetCatalog.entries.size();for(std::size_t i=0;i<a.assetCatalog.entries.size();++i)if(a.assetCatalog.entries[i].role==assets::Role::ViewHands){hands=i;break;}
        if(std::getenv("CADENCE_AUDIT_BO2_HANDS")){
            if(!assets::appendScan(a.defaultSalukiDirectory/"bo2","bo2",a.assetCatalog,error))return 2;
            hands=a.assetCatalog.entries.size();for(std::size_t i=0;i<a.assetCatalog.entries.size();++i){const auto& h=a.assetCatalog.entries[i];if(h.game=="bo2"&&h.role==assets::Role::ViewHands&&h.name.starts_with("c_usa_mp_seal6_longsleeve_viewhands")){hands=i;break;}}
            if(hands==a.assetCatalog.entries.size()){std::cerr<<"Missing BO2 reference hands\n";return 2;}
        }
        std::size_t count{};
        for(std::size_t i=0;i<a.assetCatalog.entries.size();++i){const auto asset=a.assetCatalog.entries[i];if(asset.role!=assets::Role::ViewWeapon||asset.game!=game)continue;
            if(const auto* only=std::getenv("CADENCE_AUDIT_WEAPON");only&&asset.name.find(only)==std::string::npos)continue;
            a.selectedBaseAsset=hands;try{equipViewWeapon(a,i);
                const auto idle=a.weaponProfile.animations.find("idle");
                std::cout<<game<<','<<asset.name<<",bones="<<a.scene.skeleton.bones.size()<<",clips="<<a.scene.animations.size()<<",idle="<<(idle==a.weaponProfile.animations.end()?"MISSING":idle->second)<<std::endl;
                if(a.scene.animations.empty()||idle==a.weaponProfile.animations.end()){std::cerr<<"LOAD_FAILURE "<<asset.name<<": "<<a.status<<std::endl;for(const auto& warning:a.scene.warnings)std::cerr<<warning<<std::endl;++failures;continue;}
                std::cout<<"HANDS "<<a.assetCatalog.entries[a.selectedBaseAsset].name<<std::endl;
                if(isKnifeWeapon(a)){
                    a.gameplayLogic=true;a.gameplayAction=scene::ActionRole::Melee;triggerGameplayAction(a);
                    const auto attack=findViewmodelClip(a,"melee");
                    std::cout<<"ATTACK active="<<a.actionActive<<" mapped="<<bool(attack)<<" variants="<<findViewmodelClips(a,"melee").size()<<" status="<<a.gameplayStatus<<std::endl;
                    if(!attack||!a.actionActive)++failures;
                    if(attack){const auto& clip=a.scene.animations[*attack];const auto p0=a.scene.samplePose(*attack,0),p1=a.scene.samplePose(*attack,clip.durationFrames*.4f);float delta{};for(std::size_t b=0;b<p0.size();++b)for(int k=0;k<16;++k)delta=std::max(delta,std::abs(p0[b].v[k]-p1[b].v[k]));std::cout<<"ATTACK_MOTION "<<clip.sourceName<<" delta="<<delta<<std::endl;if(delta<.001f)++failures;}
                    for(const auto variant:findViewmodelClips(a,"melee")){const auto& clip=a.scene.animations[variant];std::cout<<"ATTACK_VARIANT "<<clip.sourceName<<" frames="<<clip.durationFrames<<std::endl;if(!clip.durationFrames)++failures;}
                    stopGameplayAction(a);
                }
                std::size_t ai=0;for(std::size_t j=0;j<a.scene.animations.size();++j)if(a.scene.animations[j].sourceName==idle->second){ai=j;break;}
                if(!a.renderer.loadScene(a.scene,error)){std::cerr<<error<<std::endl;++failures;continue;}
                a.renderer.setDebugView(1);a.renderer.setViewmodelCapture(true,{.08f,.09f,.11f,1});
                const auto pose=a.scene.samplePose(ai,0);const scene::Vec3 forward{1,0,0};
                if(count<2){scene::Vec3 low{1e9f,1e9f,1e9f},high{-1e9f,-1e9f,-1e9f};
                    for(const auto& m:a.scene.meshes)for(const auto& v:m.vertices){scene::Vec3 p{};float weight{};for(std::size_t k=0;k<v.weights.size();++k)if(v.weights[k]>0&&v.bones[k]<pose.size()){p=p+scene::transformPoint(pose[v.bones[k]]*a.scene.skeleton.bones[v.bones[k]].inverseBind,v.position)*v.weights[k];weight+=v.weights[k];}if(weight==0)p=v.position;
                        low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};}
                    std::cout<<"POSE_BOUNDS "<<low.x<<','<<low.y<<','<<low.z<<" -> "<<high.x<<','<<high.y<<','<<high.z<<std::endl;
                }
                const auto vp=scene::perspective(75*scene::kPi/180,960.f/540.f,.05f,5000)*scene::lookAt({},forward,{0,0,1});
                a.renderer.render(a.scene,pose,vp,960,540,false,false,false);
                if(count++<2)(void)a.renderer.saveColorPng("diagnostics/imported_v229/"+std::string(game)+"_"+asset.name+".png",error);
            }catch(const std::exception& e){std::cerr<<game<<'/'<<asset.name<<": "<<e.what()<<std::endl;++failures;}
        }
    }
    return failures?1:0;
}
