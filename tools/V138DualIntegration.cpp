#include "assets/LocalAssetPaths.h"
static bool testLeft{},testRight{};
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
extern "C" int cadenceTestMouse(GLFWwindow*,int b){return (b==GLFW_MOUSE_BUTTON_RIGHT?testLeft:b==GLFW_MOUSE_BUTTON_LEFT?testRight:false)?GLFW_PRESS:GLFW_RELEASE;}
extern "C" int cadenceTestKey(GLFWwindow*,int){return GLFW_RELEASE;}
extern "C" int cadenceTestWindowAttribute(GLFWwindow*,int a){return a==GLFW_FOCUSED?GLFW_TRUE:0;}
extern "C" int cadenceTestInputMode(GLFWwindow*,int){return GLFW_CURSOR_DISABLED;}
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<" status="<<a.status<<" gameplay="<<a.gameplayStatus<<"\n";return 1;}}while(false)
int main(){
 if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(960,540,"Dual wield validation",nullptr,nullptr);if(!w)return 3;glfwMakeContextCurrent(w);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
 auto state=std::make_unique<AppState>();auto&a=*state;std::string error;a.window=w;a.deferSceneUpload=true;CHECK(a.renderer.initialize(error));a.defaultSalukiDirectory=cadence::local_assets::exportPath("");
 for(auto game:{"pointblank","bo2","mw3"})CHECK(assets::appendScan(a.defaultSalukiDirectory/game,game,a.assetCatalog,error));
 auto idx=[&](std::string name){for(size_t i=0;i<a.assetCatalog.entries.size();++i)if(a.assetCatalog.entries[i].name==name)return i;return a.assetCatalog.entries.size();};
 std::filesystem::create_directories("diagnostics/pointblank_v138");
 for(int variant=0;variant<2;++variant)for(auto weapon:{"t6_wpn_pistol_fnp45_view_LOD0","t6_wpn_pistol_fiveseven_view_LOD0","t6_wpn_pistol_b2023r_view_LOD0","t6_wpn_pistol_kard_view_LOD0","t6_wpn_pistol_judge_view_LOD0","viewmodel_walther_p99_iw5_LOD0","viewmodel_mp412_LOD0","viewmodel_usp45_iw5_LOD0","viewmodel_desert_eagle_iw5_LOD0","viewmodel_fn_five_seven_iw5_LOD0","viewmodel_44_magnum_iw5_LOD0","viewmodel_skorpion_iw5_LOD0","viewmodel_mp9_iw5_LOD0","viewmodel_g18_iw5_LOD0","viewmodel_fmg_iw5_LOD0","viewmodel_pistol_DesertEagle-Dual"}){
  const auto hand=std::string(weapon).starts_with("viewmodel_pistol_")?(variant?"viewmodel_SWAT_Female_SWAT-Sniper_hands":"viewmodel_SWAT_Male_hands"):(variant?"viewhands_delta_LOD0":"c_usa_mp_isa_smg_viewhands_LOD0");
  a.actorMode=false;a.activeClassSlot=-1;a.classSlotRigs={};a.selectedBaseAsset=idx(hand);const auto wi=idx(weapon);CHECK(wi<a.assetCatalog.entries.size());
  equipViewWeapon(a,wi,nullptr,true);std::cout<<"Loaded "<<weapon<<" bones="<<a.scene.skeleton.bones.size()<<" meshes="<<a.scene.meshes.size()<<" clips="<<a.scene.animations.size()<<"\n";CHECK(a.scene.dualWield);const auto idle=findViewmodelClip(a,"idle");CHECK(idle);
  a.animationIndex=*idle;a.animationFrame=0;a.actionActive=false;a.transitioning=false;a.runtimeLayers.clear();
  const auto baseline=a.scene.samplePose(*idle,0);CHECK(cadence::dual::muzzle(a.scene,baseline,0));CHECK(cadence::dual::muzzle(a.scene,baseline,1));
  a.activeClassSlot=0;a.classSlotRigs[0].emplace();a.classSlotRigs[0]->idleAnimation=0;selectViewmodelIdle(a,a.assetCatalog.entries[wi]);CHECK(a.animationIndex==*idle);a.activeClassSlot=-1;a.classSlotRigs={};
  for(auto key:{"idle","reload","reload_empty","pullout","sprint_loop"})if(auto clip=findViewmodelClip(a,key))for(int f=0;f<=60;++f){const auto p=a.scene.samplePose(*clip,a.scene.animations[*clip].durationFrames*f/60.f);for(auto&m:p)for(float v:m.v)CHECK(std::isfinite(v));}
  a.actorMode=a.actorInputCaptured=a.actorNoclip=a.playing=a.gameplayLogic=true;a.actorCapturePending=false;a.actionActive=false;a.previousFire=a.previousDualLeft=false;a.nextFireTime=a.dualLeftNextFireTime=0;a.gameplayClock=10;
  auto serial=a.acceptedShotSerial;testLeft=testRight=true;updateActorController(a,1.f/165);CHECK(a.acceptedShotSerial==serial+2);CHECK(a.runtimeLayers.size()==2);CHECK(a.pendingDualShots.size()==2);CHECK(!a.gameplayAds);updateActorController(a,1.f/165);CHECK(a.acceptedShotSerial==serial+2);
  testLeft=testRight=false;updateActorController(a,1.f/165);a.dualBurstRemaining={};a.gameplayClock+=1;testLeft=true;updateActorController(a,1.f/165);CHECK(a.acceptedShotSerial==serial+3);testLeft=false;
  a.actorMode=false;a.runtimeLayers.clear();a.actionActive=false;a.animationIndex=*idle;a.animationFrame=0;a.transitioning=false;
  CHECK(a.renderer.loadScene(a.scene,error));a.renderer.setViewmodelCapture(true,{.035f,.04f,.045f,1});a.renderer.setDebugView(1);
  const auto cameraIndex=a.scene.skeleton.boneByCanonicalName.at("tag_camera");const auto camera=baseline[cameraIndex];const scene::Vec3 eye{camera.v[12],camera.v[13],camera.v[14]},forward{camera.v[0],camera.v[1],camera.v[2]},up{camera.v[8],camera.v[9],camera.v[10]};const auto vp=scene::perspective(scene::kPi*65/180,16.f/9,.1f,2000)*scene::lookAtDirection(eye,forward,up);a.renderer.setCameraPosition(eye);a.renderer.render(a.scene,baseline,vp,960,540,false,false,false);const auto out=std::filesystem::path("diagnostics/pointblank_v138")/(std::string(weapon)+"_"+hand);CHECK(a.renderer.saveColorPng(out.string()+".png",error));
  for(auto key:{"fire_left","fire_right","reload","pullout"})if(auto c=findViewmodelClip(a,key))for(int f=0;f<5;++f){scene::PoseSlot slot;slot.nodes.push_back({*c,a.scene.animations[*c].durationFrames*f/4.f,1,scene::LayerMode::Override,false});auto p=a.scene.samplePoseSlots(*idle,0,{slot});a.renderer.render(a.scene,p,vp,960,540,false,false,false);CHECK(a.renderer.saveColorPng(out.string()+"_"+key+"_"+std::to_string(f)+".png",error));}
  const auto bones=a.scene.skeleton.bones.size(),meshes=a.scene.meshes.size();std::vector<scene::Mat4> binds;for(auto&b:a.scene.skeleton.bones)binds.push_back(b.inverseBind);captureTakeActorManifest(a);const auto manifest=a.recordedTake.actor;CHECK(restoreTakeActor(a,manifest,bones,error));CHECK(a.scene.meshes.size()==meshes);for(size_t b=0;b<bones;++b)for(int k=0;k<16;++k)CHECK(std::abs(binds[b].v[k]-a.scene.skeleton.bones[b].inverseBind.v[k])<.002f);
  std::cout<<"PASS "<<weapon<<" independent fire, fractional poses, muzzle sockets, replay geometry\n";
  if(variant==0&&std::string(weapon)=="t6_wpn_pistol_fnp45_view_LOD0"){a.activeClassSlot=0;a.classSlotRigs[0].emplace();a.classSlotRigs[0]->idleAnimation=0;equipViewWeapon(a,wi,nullptr,false);CHECK(!a.scene.dualWield);auto normal=findViewmodelClip(a,"idle");CHECK(normal&&a.animationIndex==*normal);CHECK(a.scene.animations[*normal].sourceName.find("cadence_dual")==std::string::npos);a.activeClassSlot=-1;a.classSlotRigs={};}
 }
 a.renderer.shutdown();state.reset();ImGui::DestroyContext();glfwDestroyWindow(w);glfwTerminate();return 0;
}
