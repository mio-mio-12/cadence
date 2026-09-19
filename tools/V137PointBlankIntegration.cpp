static bool testAds{},testFire{};
#define glfwGetMouseButton cadenceTestMouse
#define glfwGetKey cadenceTestKey
#define glfwGetWindowAttrib cadenceTestWindowAttribute
#define glfwGetInputMode cadenceTestInputMode
#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#undef glfwGetMouseButton
#undef glfwGetKey
#undef glfwGetWindowAttrib
#undef glfwGetInputMode
extern "C" int cadenceTestMouse(GLFWwindow*,int button){return (button==GLFW_MOUSE_BUTTON_RIGHT?testAds:button==GLFW_MOUSE_BUTTON_LEFT?testFire:false)?GLFW_PRESS:GLFW_RELEASE;}
extern "C" int cadenceTestKey(GLFWwindow*,int){return GLFW_RELEASE;}
extern "C" int cadenceTestWindowAttribute(GLFWwindow*,int attribute){return attribute==GLFW_FOCUSED?GLFW_TRUE:0;}
extern "C" int cadenceTestInputMode(GLFWwindow*,int){return GLFW_CURSOR_DISABLED;}
#include <fstream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<" status="<<a.gameplayStatus<<"\n";return 1;}}while(false)
int main(){
 if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(960,540,"PB integration",nullptr,nullptr);if(!w)return 3;glfwMakeContextCurrent(w);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
 auto state=std::make_unique<AppState>();auto&a=*state;std::string error;a.window=w;a.deferSceneUpload=true;CHECK(a.renderer.initialize(error));a.defaultSalukiDirectory="D:/Editing/COD Resource/3D Rip/saluki/exported_files";
 for(auto game:{"pointblank","bo2"})CHECK(assets::appendScan(a.defaultSalukiDirectory/game,game,a.assetCatalog,error));
 auto idx=[&](std::string name){for(size_t i=0;i<a.assetCatalog.entries.size();++i)if(a.assetCatalog.entries[i].name==name)return i;return a.assetCatalog.entries.size();};
 std::filesystem::create_directories("diagnostics/pointblank_v137");std::ofstream report("diagnostics/pointblank_v137/integration.txt");
 for(auto hand:{"viewmodel_SWAT_Male_hands","c_usa_mp_isa_smg_viewhands_LOD0"})for(auto weapon:{"viewmodel_sniper_DSR_1","viewmodel_knife_Kunai_Dual","viewmodel_pistol_DesertEagle","viewmodel_sniper_Cheytac_M200","viewmodel_sniper_Taclite-T2","viewmodel_pistol_ColtPython","viewmodel_knife_M-9_Dual","viewmodel_knife_M-9_Dual_PBNC"}){
  a.actorMode=false;a.selectedBaseAsset=idx(hand);const auto wi=idx(weapon);CHECK(wi<a.assetCatalog.entries.size());equipViewWeapon(a,wi,nullptr);CHECK(a.selectedWeaponAsset==wi);a.gameplayLogic=a.playing=true;a.runtimeLayers.clear();a.previousAds=a.previousFire=false;a.nextFireTime=0;a.gameplayClock=10;a.pendingWeaponRechamber=false;
  const auto idle=findViewmodelClip(a,"idle");CHECK(idle);const auto baseline=a.scene.samplePose(*idle,0);
  for(auto action:{scene::ActionRole::Reload,scene::ActionRole::Equip,scene::ActionRole::Melee}){
   const auto key=action==scene::ActionRole::Reload?"reload":action==scene::ActionRole::Equip?"pullout":"melee";if(!findViewmodelClip(a,key))continue;
   a.gameplayAds=a.viewmodelAdsEngaged=false;a.gameplayAction=action;triggerGameplayAction(a);CHECK(a.actionActive&&a.actionOverlay);a.transitioning=false;a.animationIndex=*idle;a.animationFrame=0;
   const float duration=a.actionDurationOverride>0?a.actionDurationOverride:authoredClipDuration(a,a.actionAnimationIndex);
   for(int f=0;f<=120;++f){a.actionElapsed=duration*f/120.f;a.actionFrame=a.scene.animations[a.actionAnimationIndex].durationFrames*f/120.f;auto pose=evaluateCurrentPose(a);for(auto&m:pose)for(float v:m.v)CHECK(std::isfinite(v));if(f==120)for(size_t b=0;b<baseline.size();++b)for(int k=0;k<16;++k)CHECK(std::abs(pose[b].v[k]-baseline[b].v[k])<.002f);}
   stopGameplayAction(a);
  }
  if(weapon::isSniper(a.weaponProfile.archetype)){
   a.actorMode=a.actorInputCaptured=true;a.actorNoclip=true;a.actorCapturePending=false;a.actionActive=false;a.previousAds=a.previousFire=false;testAds=true;testFire=false;updateActorController(a,1.f/165);
   CHECK(a.gameplayAds&&a.viewmodelAdsEngaged&&a.adsCameraBlend>.999f&&a.weaponTiming.hideWeaponOnAds);CHECK(!a.actionActive||a.activeAction!=scene::ActionRole::Aim);
   auto serial=a.acceptedShotSerial;testFire=true;updateActorController(a,1.f/165);CHECK(a.acceptedShotSerial==serial+1);
   testFire=false;testAds=false;updateActorController(a,1.f/165);CHECK(!a.viewmodelAdsEngaged&&a.adsCameraBlend<.001f);a.actorMode=a.actorInputCaptured=false;
  }
  if(weapon::isSniper(a.weaponProfile.archetype)&&findViewmodelClip(a,"rechamber")){
   a.actorMode=false;a.gameplayAds=a.viewmodelAdsEngaged=false;a.runtimeLayers.clear();a.transitioning=false;a.actionActive=false;a.interruptPoseDuration=0;a.animationIndex=*idle;a.animationFrame=0;
   const auto source=evaluateCurrentPose(a);beginInterruptPoseBlend(a,.08f);a.gameplayRechamber=true;a.gameplayAction=scene::ActionRole::Reload;triggerGameplayAction(a);CHECK(a.actionActive&&a.actionOverlay);
   const auto entry=evaluateCurrentPose(a);for(size_t b=0;b<entry.size();++b)for(int k=0;k<16;++k)CHECK(std::abs(source[b].v[k]-entry[b].v[k])<.002f);
   a.actionElapsed=.04f;a.actionFrame=.04f*a.scene.animations[a.actionAnimationIndex].framerate;a.interruptPoseElapsed=.04f;
   const auto halfway=evaluateCurrentPose(a);a.interruptPoseDuration=0;const auto destination=evaluateCurrentPose(a);
   for(size_t b=0;b<halfway.size();++b){const auto expected=cadence::codm_actions::blendAffine(source[b],destination[b],.5f);for(int k=0;k<16;++k)CHECK(std::abs(expected.v[k]-halfway[b].v[k])<.002f);}
   stopGameplayAction(a);
  }
  const auto bones=a.scene.skeleton.bones.size();std::vector<scene::Mat4> referenceBinds;for(const auto&b:a.scene.skeleton.bones)referenceBinds.push_back(b.inverseBind);
  captureTakeActorManifest(a);const auto manifest=a.recordedTake.actor;CHECK(restoreTakeActor(a,manifest,bones,error));CHECK(a.scene.skeleton.bones.size()==bones);
  for(size_t b=0;b<bones;++b)for(int k=0;k<16;++k)if(std::abs(referenceBinds[b].v[k]-a.scene.skeleton.bones[b].inverseBind.v[k])>=.002f){std::cerr<<hand<<" / "<<weapon<<" bind "<<a.scene.skeleton.bones[b].name<<" component "<<k<<" before "<<referenceBinds[b].v[k]<<" after "<<a.scene.skeleton.bones[b].inverseBind.v[k]<<"\n";CHECK(false);}
  report<<"PASS "<<hand<<" / "<<weapon<<" actions, finite fractional poses, idle endpoint, scope/fire/release (snipers), replay reconstruction\n";report.flush();
 }
 a.renderer.shutdown();state.reset();ImGui::DestroyContext();glfwDestroyWindow(w);glfwTerminate();std::cout<<"Point Blank integration passed\n";
}
