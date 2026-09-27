#include "assets/LocalAssetPaths.h"
#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<" "<<error<<'\n';return 1;}}while(false)
int main(){
 std::string error;CHECK(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(640,360,"Replay check",nullptr,nullptr);CHECK(w);glfwMakeContextCurrent(w);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
 auto state=std::make_unique<AppState>();auto&a=*state;a.window=w;a.deferSceneUpload=true;CHECK(a.renderer.initialize(error));a.defaultSalukiDirectory=cadence::local_assets::exportPath("");
 for(auto game:{"pointblank","bo2"})CHECK(assets::appendScan(a.defaultSalukiDirectory/game,game,a.assetCatalog,error));
 auto idx=[&](std::string name){for(size_t i=0;i<a.assetCatalog.entries.size();++i)if(a.assetCatalog.entries[i].name==name)return i;return a.assetCatalog.entries.size();};
 for(auto hand:{"viewmodel_SWAT_Male_hands","c_usa_mp_isa_smg_viewhands_LOD0"})for(auto weapon:{"viewmodel_pistol_DesertEagle","viewmodel_pistol_ColtPython","viewmodel_knife_M-9_Dual"}){
  a.selectedBaseAsset=idx(hand);const auto wi=idx(weapon);CHECK(a.selectedBaseAsset<a.assetCatalog.entries.size()&&wi<a.assetCatalog.entries.size());
  equipViewWeapon(a,wi);a.actorMode=true;a.gameplayLogic=true;a.actorViewHeight=60;a.actorRenderPosition={0,0,0};a.actorPosition={0,0,0};a.actorPresentationZValid=false;a.activeClassSlot=-1;
  a.weaponProfile.gunPosition={2,3,4};a.weaponProfile.adsGunPosition={5,6,7};a.weaponProfile.separateAdsPosition=true;
  const auto idle=findViewmodelClip(a,"idle");CHECK(idle);a.animationIndex=*idle;a.animationFrame=0;a.actionActive=false;
  const auto live=evaluateCurrentPose(a);a.recordedTake.clear();a.recordedTake.boneCount=a.scene.skeleton.bones.size();captureTakeActorManifest(a);captureTakeSample(a,0);
  CHECK(a.recordedTake.neutralGunPosition&&a.recordedTake.samples.size()==1);
  auto replay=a.recordedTake.samples[0].pose;const auto camera=a.scene.skeleton.boneByCanonicalName.at("tag_camera");
  cadence::replay::applyMountEdit(replay,camera,{{},{},false,true},a.weaponProfile,0,scene::inverseAffine(a.scene.skeleton.bones[camera].restGlobal));
  float positionError=0;for(size_t b=0;b<live.size();++b)if(b!=camera)for(int k=12;k<15;++k)positionError=std::max(positionError,std::abs(live[b].v[k]-replay[b].v[k]));
  std::cout<<hand<<" / "<<weapon<<" mount error="<<positionError<<std::endl;
  CHECK(positionError<.002f);
  for(float blend:{.25f,.5f,1.f}){
   a.adsCameraBlend=blend;const auto expected=evaluateCurrentPose(a);
   auto actual=evaluateCurrentPose(a,true);actual[camera]=a.actorViewCamera;
   cadence::replay::applyMountEdit(actual,camera,{{},{},false,true},a.weaponProfile,blend,scene::inverseAffine(a.scene.skeleton.bones[camera].restGlobal));
   for(size_t b=0;b<expected.size();++b)if(b!=camera)for(int k=12;k<15;++k)CHECK(std::abs(expected[b].v[k]-actual[b].v[k])<.002f);
  }
  a.adsCameraBlend=0;
  const auto before=a.scene;const auto manifest=a.recordedTake.actor;
  CHECK(restoreTakeActor(a,manifest,before.skeleton.bones.size(),error));
  float worst=0;std::string bad;
  for(size_t b=0;b<before.skeleton.bones.size();++b){CHECK(before.skeleton.bones[b].name==a.scene.skeleton.bones[b].name);for(int k=0;k<16;++k){auto diff=std::abs(before.skeleton.bones[b].inverseBind.v[k]-a.scene.skeleton.bones[b].inverseBind.v[k]);if(diff>worst){worst=diff;bad=before.skeleton.bones[b].name;}}}
  std::cout<<hand<<" / "<<weapon<<" bind error="<<worst<<" bone="<<bad<<std::endl;CHECK(worst<.002f);
  CHECK(a.weaponProfile.gunPosition.x==2&&a.weaponProfile.adsGunPosition.z==7);
  prewarmTakePlayback(a);CHECK(a.weaponProfile.gunPosition.x==2&&a.weaponProfile.adsGunPosition.z==7);
  a.takePreview=true;CHECK(ensureTakeWeaponSlot(a,0));CHECK(a.weaponProfile.gunPosition.x==2);
  a.liveClassBeforeTake=a.classSlotRigs;
  a.weaponProfile.gunPosition.x=19;a.weaponProfile.adsGunPosition.z=23;syncSharedGunPosition(a);
  CHECK(a.classSlotRigs[0]->profile.gunPosition.x==19&&(*a.liveClassBeforeTake)[0]->profile.adsGunPosition.z==23);
  a.liveClassBeforeTake.reset();
  a.classSlotRigs={};a.classGpuResident=false;a.activeClassSlot=-1;a.takePreview=false;
 }
 std::cout<<"Replay reconstruction passed\n";
}
