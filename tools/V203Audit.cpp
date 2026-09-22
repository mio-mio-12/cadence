#include "assets/LocalAssetPaths.h"
#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main
int main(){
 if(!glfwInit())return 1;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(900,700,"v203 audit",nullptr,nullptr);if(!w)return 2;glfwMakeContextCurrent(w);
 auto state=std::make_unique<AppState>();auto&a=*state;a.defaultSalukiDirectory=cadence::local_assets::exportPath("");std::string error;
 if(!a.renderer.initialize(error))return 3;std::filesystem::create_directories("diagnostics/v203");std::ofstream log("diagnostics/v203/results.txt");
 for(auto*g:{"codm","bo2"})(void)assets::appendScan(a.defaultSalukiDirectory/g,g,a.assetCatalog,error);
 PointBlankPreparationBatch batch;
 for(size_t i=0;i<a.assetCatalog.entries.size();++i){const auto&e=a.assetCatalog.entries[i];if(e.role!=assets::Role::PlayerModel||e.name.find("Reaper_Ashura")==std::string::npos)continue;
 a.classWorldModelAsset=i;a.hiddenWorldActor=scene::buildScene(cast::Document::load(e.path));auto&s=*a.hiddenWorldActor;appendClassBodyAnimations(a,"codm",s);
 for(const auto&m:s.meshes)log<<m.materialName<<" alpha="<<m.forceAlpha<<" ignore="<<m.ignoreAlbedoAlpha<<" texture="<<m.albedoPath.filename()<<std::endl;
 if(!a.renderer.loadScene(s,error))return 4;a.renderer.setDebugView(1);
 a.actorMode=true;a.actorGrounded=true;a.actorMoveInputForward=1;a.actorVelocity=a.actorWishVelocity={120,0,0};a.gameplayWeapon=scene::WeaponClass::Rifle;a.gameplayStance=scene::Stance::Stand;
 for(bool ads:{false,true}){a.gameplayAds=ads;for(int k=0;k<12;++k){a.gameplayClock=ads?3+k*.1f:1+k*.1f;std::string primary,torso;float frame=0;auto pose=evaluateHiddenWorldActorPose(a,&primary,&frame,&torso);auto ankle=s.skeleton.boneByCanonicalName.at("j_ankle_le");log<<"ads="<<ads<<" time="<<a.gameplayClock<<" base="<<primary<<" frame="<<frame<<" torso="<<torso<<" ankle="<<pose[ankle].v[12]<<","<<pose[ankle].v[13]<<","<<pose[ankle].v[14]<<std::endl;
 auto vp=scene::perspective(45*scene::kPi/180,1.3f,1,4000)*scene::lookAt({250,-280,160},{0,0,90},{0,0,1});a.renderer.render(s,pose,vp,900,700,false,false,false);if(k%3==0)(void)a.renderer.saveColorPng("diagnostics/v203/"+std::string(ads?"ads":"hip")+std::to_string(k)+".png",error);
 }}break;}
 return 0;
}
