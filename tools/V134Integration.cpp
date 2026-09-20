#include "assets/LocalAssetPaths.h"
#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<" status="<<a.status<<" error="<<error<<"\n";return 1;}}while(false)
int main(){
 if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(960,540,"Point Blank integration",nullptr,nullptr);if(!w)return 3;glfwMakeContextCurrent(w);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
 auto state=std::make_unique<AppState>();auto&a=*state;std::string error;a.window=w;a.deferSceneUpload=true;CHECK(a.renderer.initialize(error));
 a.defaultSalukiDirectory=cadence::local_assets::exportPath("");
 CHECK(assets::appendScan(a.defaultSalukiDirectory/"pointblank","pointblank",a.assetCatalog,error));CHECK(assets::appendScan(a.defaultSalukiDirectory/"bo2","bo2",a.assetCatalog,error));
 auto idx=[&](std::string n){for(size_t i=0;i<a.assetCatalog.entries.size();++i)if(lowerText(a.assetCatalog.entries[i].name)==lowerText(n))return i;return a.assetCatalog.entries.size();};
 const auto weapon=idx("viewmodel_sniper_XM2010");CHECK(weapon<a.assetCatalog.entries.size());
 for(auto hand:{"viewmodel_SWAT_Male_hands","c_usa_mp_isa_smg_viewhands_LOD0"}){
  a.selectedBaseAsset=idx(hand);CHECK(a.selectedBaseAsset<a.assetCatalog.entries.size());equipViewWeapon(a,weapon,nullptr);CHECK(!a.scene.pointBlankWeaponStem.empty());CHECK(!a.scene.animations.empty());CHECK(a.weaponTiming.adsIn==0&&a.weaponTiming.adsOut==0);CHECK(a.weaponTiming.boltAction);CHECK(findViewmodelClip(a,"jump_takeoff")&&findViewmodelClip(a,"jump_land"));CHECK(!findViewmodelClip(a,"sprint_loop"));
  a.gameplayLogic=true;
  for(auto action:{scene::ActionRole::Fire,scene::ActionRole::Reload,scene::ActionRole::Equip}){a.gameplayAction=action;triggerGameplayAction(a);CHECK(a.actionActive);for(int i=0;i<=60;++i){a.actionElapsed=i/60.f*(a.actionDurationOverride>0?a.actionDurationOverride:authoredClipDuration(a,a.actionAnimationIndex));a.actionFrame=a.actionElapsed*a.scene.animations[a.actionAnimationIndex].framerate;if(!a.actionOverlay)a.animationFrame=a.actionFrame;const auto p=evaluateCurrentPose(a);CHECK(p.size()==a.scene.skeleton.bones.size());for(auto&m:p)for(float v:m.v)CHECK(std::isfinite(v));}stopGameplayAction(a);}
  a.jumpFeedbackAnimation=*findViewmodelClip(a,"jump_takeoff");a.jumpFeedbackElapsed=.2f;auto jumpPose=evaluateCurrentPose(a);CHECK(!jumpPose.empty());
  const auto bones=a.scene.skeleton.bones.size();const auto meshes=a.scene.meshes.size();captureTakeActorManifest(a);auto manifest=a.recordedTake.actor;
  CHECK(restoreTakeActor(a,manifest,bones,error));CHECK(a.scene.skeleton.bones.size()==bones&&a.scene.meshes.size()==meshes);CHECK(!a.scene.pointBlankWeaponStem.empty());
  std::cout<<"PASS "<<hand<<" action mappings, fractional blends, jump layer, replay actor reconstruction ("<<bones<<" bones)\n";
 }
 a.renderer.shutdown();state.reset();ImGui::DestroyContext();glfwDestroyWindow(w);glfwTerminate();return 0;
}
