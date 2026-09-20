#include "assets/LocalAssetPaths.h"
#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <fstream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<" "<<a.gameplayStatus<<"\n";return 1;}}while(false)
static float difference(const std::vector<scene::Mat4>&a,const std::vector<scene::Mat4>&b){float d=0;for(size_t i=0;i<a.size();++i)for(int k=0;k<16;++k)d=std::max(d,std::abs(a[i].v[k]-b[i].v[k]));return d;}
int main(){
 if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(960,540,"v139 checks",nullptr,nullptr);if(!w)return 3;glfwMakeContextCurrent(w);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
 auto state=std::make_unique<AppState>();auto&a=*state;std::string error;a.window=w;a.deferSceneUpload=true;CHECK(a.renderer.initialize(error));a.defaultSalukiDirectory=cadence::local_assets::exportPath("");
 for(auto g:{"pointblank","bo2","codm"})CHECK(assets::appendScan(a.defaultSalukiDirectory/g,g,a.assetCatalog,error));
 auto idx=[&](const char*name){for(size_t i=0;i<a.assetCatalog.entries.size();++i)if(a.assetCatalog.entries[i].name==name)return i;return a.assetCatalog.entries.size();};
 const auto out=std::filesystem::path("diagnostics/v139");std::filesystem::create_directories(out);std::ofstream report(out/"transitions.txt");
 auto reset=[&]{a.actorMode=false;a.gameplayLogic=a.playing=true;a.actionActive=a.actionOverlay=false;a.runtimeLayers.clear();a.transitioning=false;a.gameplayAds=a.viewmodelAdsEngaged=a.viewmodelAdsExiting=false;a.gameplayRechamber=false;a.pendingWeaponRechamber=false;a.interruptPoseDuration=0;a.jumpFeedbackAnimation=SIZE_MAX;};
 auto render=[&](const std::vector<scene::Mat4>&p,const std::string&name){const auto c=p[a.scene.skeleton.boneByCanonicalName.at("tag_camera")];scene::Vec3 eye{c.v[12],c.v[13],c.v[14]},forward{c.v[0],c.v[1],c.v[2]},up{c.v[8],c.v[9],c.v[10]};const auto vp=scene::perspective(65*scene::kPi/180,16.f/9,.1f,2000)*scene::lookAtDirection(eye,forward,up);a.renderer.setCameraPosition(eye);a.renderer.render(a.scene,p,vp,960,540,false,false,false);return a.renderer.saveColorPng(out/(name+".png"),error);};
 for(auto hand:{"viewmodel_SWAT_Male_hands","viewmodel_SWAT_Female_SWAT-Sniper_hands","c_usa_mp_isa_smg_viewhands_LOD0"})for(auto weapon:{"viewmodel_pistol_DesertEagle-Dual","viewmodel_pistol_DesertEagle","viewmodel_knife_M-9_Dual"}){
  reset();a.selectedBaseAsset=idx(hand);const auto wi=idx(weapon);CHECK(wi<a.assetCatalog.entries.size());equipViewWeapon(a,wi);reset();auto idle=findViewmodelClip(a,"idle"),draw=findViewmodelClip(a,"pullout");CHECK(idle&&draw);a.animationIndex=*idle;a.animationFrame=0;
  const auto baseline=evaluateCurrentPose(a);a.jumpFeedbackAnimation=0;a.jumpFeedbackElapsed=.05f;a.actorGrounded=false;a.gameplayAction=scene::ActionRole::Equip;triggerGameplayAction(a);CHECK(a.actionActive&&a.actionOverlay);CHECK(a.jumpFeedbackAnimation==SIZE_MAX);CHECK(!a.transitioning);
  const auto first=evaluateCurrentPose(a);auto expected=a.scene.samplePoseSlots(*idle,0,{{{{*draw,0,1,scene::LayerMode::Override,false}}}});CHECK(difference(first,expected)<.002f);CHECK(difference(first,baseline)>.1f);
  CHECK(a.renderer.loadScene(a.scene,error));a.renderer.setViewmodelCapture(true,{.035f,.04f,.045f,1});a.renderer.setDebugView(1);
  const std::string prefix=std::string(weapon)+"_"+hand;
  for(int f=0;f<=4;++f){a.actionFrame=a.scene.animations[*draw].durationFrames*f/4.f;a.actionElapsed=authoredClipDuration(a,*draw)*f/4.f;auto p=evaluateCurrentPose(a);for(auto&m:p)for(auto v:m.v)CHECK(std::isfinite(v));CHECK(render(p,prefix+"_draw_"+std::to_string(f)));}
  reset();a.animationIndex=*idle;a.animationFrame=0;
  if(a.scene.dualWield){CHECK(findViewmodelClip(a,"jump_takeoff")&&findViewmodelClip(a,"jump_land"));
   for(int side=0;side<2;++side){auto fire=a.scene.dualFireClips[side];const auto&clip=a.scene.animations[fire];a.runtimeLayers={{scene::ActionRole::Fire,fire,0,0,authoredClipDuration(a,fire),1,0,.08f,scene::LayerMode::Override,false,1}};CHECK(difference(evaluateCurrentPose(a),baseline)<.002f);
    float slideMotion=0;const auto bone=a.scene.skeleton.boneByName.at(side?"pb2cast_weapon__left__HandleAnimationDummy":"pb2cast_weapon__HandleAnimationDummy");const auto ref=a.scene.sampleLocalPose(fire,0)[bone].position;
    for(int f=0;f<=60;++f){const float frame=clip.durationFrames*f/60.f;slideMotion=std::max(slideMotion,scene::length(a.scene.sampleLocalPose(fire,frame)[bone].position-ref));a.runtimeLayers[0].frame=frame;a.runtimeLayers[0].elapsed=frame/clip.framerate;auto p=evaluateCurrentPose(a);for(auto&m:p)for(auto v:m.v)CHECK(std::isfinite(v));if(f%15==0)CHECK(render(p,prefix+"_fire"+std::to_string(side)+"_"+std::to_string(f)));}
    report<<prefix<<" side="<<side<<" slide motion cm="<<slideMotion<<"\n";report.flush();CHECK(slideMotion>.1f);a.runtimeLayers.clear();
   }
  }
  if(std::string(weapon)=="viewmodel_pistol_DesertEagle"&&std::string(hand)=="viewmodel_SWAT_Male_hands"){
   a.renderer.setDebugView(0);a.renderer.setSun(true,false,{.3f,.7f,-1},1,.35f,{1,1,1},{1,1,1},1024,1000,500,{},false,1024,1000,2000,100);
   std::vector<uint8_t> original,metal,restored;
   a.renderer.setWeaponMaterialTuning(1,{},{1,1,1},1,.08f,-1);CHECK(render(baseline,"metalness_disabled"));CHECK(a.renderer.readColorRgba(original,error));
   a.renderer.setWeaponMaterialTuning(1,{},{1,1,1},1,.08f,1);CHECK(render(baseline,"metalness_one"));CHECK(a.renderer.readColorRgba(metal,error));CHECK(original!=metal);
   a.renderer.setWeaponMaterialTuning(1,{},{1,1,1},1,.08f,-1);CHECK(render(baseline,"metalness_restored"));CHECK(a.renderer.readColorRgba(restored,error));CHECK(original==restored);a.renderer.setDebugView(1);
   report<<"PASS metalness shader changes output and disabled override restores exact pixels\n";
  }
  report<<"PASS "<<prefix<<" midair draw frame zero, finite poses, shot entry and slide\n";report.flush();
 }
 for(auto hand:{"codm_viewhands_c_m_ghost_Default","c_usa_mp_isa_smg_viewhands_LOD0"})for(auto weapon:{"viewmodel_shotty_m1887_Default","viewmodel_sniper_locus_scuba"}){
  reset();a.selectedBaseAsset=idx(hand);const auto wi=idx(weapon);CHECK(wi<a.assetCatalog.entries.size());equipViewWeapon(a,wi);reset();auto idle=findViewmodelClip(a,"idle"),up=findViewmodelClip(a,"ads_up"),down=findViewmodelClip(a,"ads_down"),bolt=findViewmodelClip(a,"ads_rechamber");CHECK(idle&&up&&down&&bolt);a.animationIndex=*idle;a.animationFrame=0;
  CHECK(startViewmodelAimClip(a,*up));a.viewmodelAdsBaseFrame=a.scene.animations[*up].durationFrames;a.viewmodelAdsTransitionElapsed=a.weaponTiming.adsIn;a.gameplayAds=true;a.gameplayRechamber=true;a.gameplayAction=scene::ActionRole::Reload;triggerGameplayAction(a);CHECK(a.actionOverlay);const auto action=a.actionAnimationIndex;CHECK(action==*bolt);a.actionFrame=a.scene.animations[action].durationFrames*.4f;a.actionElapsed=authoredClipDuration(a,action)*.4f;a.interruptPoseDuration=0;a.transitioning=false;
  const auto before=evaluateCurrentPose(a);const float frame=a.actionFrame,elapsed=a.actionElapsed,duration=a.actionDurationOverride;CHECK(startViewmodelAimClip(a,*down));a.actionActive=a.actionOverlay=true;a.activeAction=scene::ActionRole::Reload;a.actionAnimationIndex=action;a.actionFrame=frame;a.actionElapsed=elapsed;a.actionDurationOverride=duration;a.gameplayAds=false;CHECK(a.codmReleasedFireReference==*up);a.interruptPoseElapsed=0;
  const auto first=evaluateCurrentPose(a);CHECK(difference(before,first)<.01f);a.interruptPoseElapsed=a.interruptPoseDuration;std::vector<scene::Mat4> last;
  CHECK(a.renderer.loadScene(a.scene,error));
  for(int f=0;f<=4;++f){a.viewmodelAdsBaseFrame=a.scene.animations[*down].durationFrames*f/4.f;auto p=evaluateCurrentPose(a);for(auto&m:p)for(auto v:m.v)CHECK(std::isfinite(v));last=p;CHECK(render(p,std::string(weapon)+"_"+hand+"_bolt_release_"+std::to_string(f)));}
  CHECK(difference(first,last)>.1f);a.viewmodelAdsEngaged=a.viewmodelAdsExiting=false;CHECK(difference(evaluateCurrentPose(a),last)<.002f);CHECK(a.actionFrame==frame&&a.actionElapsed==elapsed);
  report<<"PASS "<<weapon<<" / "<<hand<<" ADS release during bolt, entry/exit continuity and unchanged action clock\n";report.flush();
 }
 a.renderer.shutdown();state.reset();ImGui::DestroyContext();glfwDestroyWindow(w);glfwTerminate();std::cout<<"v139 transitions passed\n";
}
