#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <fstream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<" status="<<a.status<<" error="<<error<<"\n";return 1;}}while(false)
int main(){
 if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(960,540,"Point Blank knife integration",nullptr,nullptr);if(!w)return 3;glfwMakeContextCurrent(w);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
 auto state=std::make_unique<AppState>();auto&a=*state;std::string error;a.window=w;a.deferSceneUpload=true;CHECK(a.renderer.initialize(error));a.renderer.setViewmodelCapture(true,{.035f,.04f,.045f,1});a.renderer.setDebugView(1);
 a.defaultSalukiDirectory="D:/Editing/COD Resource/3D Rip/saluki/exported_files";
 for(auto game:{"pointblank","bo2","mw","mw3","ghosts","aw","iw_sp","mwr"})CHECK(assets::appendScan(a.defaultSalukiDirectory/game,game,a.assetCatalog,error));
 std::vector<size_t> hands,weapons;
 auto idx=[&](std::string n){for(size_t i=0;i<a.assetCatalog.entries.size();++i)if(lowerText(a.assetCatalog.entries[i].name)==lowerText(n))return i;return a.assetCatalog.entries.size();};
 hands.push_back(idx("viewmodel_SWAT_Male_hands"));hands.push_back(idx("c_usa_mp_isa_smg_viewhands_LOD0"));
 for(auto game:{"mw","mw3","ghosts","aw","iw_sp","mwr"})for(size_t i=0;i<a.assetCatalog.entries.size();++i)if(a.assetCatalog.entries[i].game==game&&a.assetCatalog.entries[i].role==assets::Role::ViewHands&&(std::string(game)!="iw_sp"||a.assetCatalog.entries[i].name=="viewmodel_base_viewhands_iw7_LOD0")){hands.push_back(i);break;}
 for(size_t i=0;i<a.assetCatalog.entries.size();++i)if(a.assetCatalog.entries[i].game=="pointblank"&&a.assetCatalog.entries[i].role==assets::Role::ViewWeapon&&lowerText(a.assetCatalog.entries[i].name).starts_with("viewmodel_knife_"))weapons.push_back(i);
 CHECK(weapons.size()==4&&hands.size()==8);std::filesystem::path output="diagnostics/pointblank_v135/integration";std::filesystem::create_directories(output);std::ofstream report(output/"report.txt");
 for(auto hand:hands)for(auto weapon:weapons){CHECK(hand<a.assetCatalog.entries.size());a.selectedBaseAsset=hand;equipViewWeapon(a,weapon,nullptr);CHECK(a.selectedWeaponAsset==weapon&&a.loadedBaseModelPath==a.assetCatalog.entries[hand].path);CHECK(a.scene.pointBlankWeaponStem==a.assetCatalog.entries[weapon].name);a.gameplayLogic=true;a.playing=true;a.weaponTiming.meleeTime=0;a.attackVariantCursor=0;CHECK(a.weaponProfile.meleeWeapon);auto variants=findViewmodelClips(a,"melee");CHECK(!variants.empty());CHECK(a.renderer.loadScene(a.scene,error));
  const auto idle=findViewmodelClip(a,"idle");CHECK(idle);std::set<size_t> visited;const auto game=a.assetCatalog.entries[hand].game,weaponName=a.assetCatalog.entries[weapon].name;
  auto capture=[&](const std::vector<scene::Mat4>&pose,const std::string&label){for(int side=0;side<2;++side){scene::Vec3 eye=side?scene::Vec3{180,40,155}:scene::Vec3{0,0,165};auto target=side?scene::Vec3{0,40,145}:eye+scene::Vec3{0,1,0};a.renderer.setCameraPosition(eye);auto vp=scene::perspective(scene::kPi*55/180,16.f/9,.1f,1000)*scene::lookAt(eye,target,{0,0,1});a.renderer.render(a.scene,pose,vp,960,540,false,false,false);if(!a.renderer.saveColorPng(output/(game+"_"+weaponName+"_"+label+(side?"_side.png":"_view.png")),error))throw std::runtime_error(error);}};
  for(size_t v=0;v<variants.size()+1;++v){a.gameplayAction=scene::ActionRole::Melee;triggerGameplayAction(a);CHECK(a.actionActive&&a.actionOverlay);const auto ai=a.actionAnimationIndex;CHECK(ai==variants[v%variants.size()]);visited.insert(ai);const float duration=authoredClipDuration(a,ai);CHECK(a.actionDurationOverride==0);CHECK(!a.scene.animations[ai].looping);
   for(int f=0;f<=120;++f){a.actionElapsed=duration*f/120.f;a.actionFrame=a.actionElapsed*a.scene.animations[ai].framerate;a.animationFrame=0;a.transitioning=false;auto pose=evaluateCurrentPose(a);for(auto&m:pose)for(float value:m.v)CHECK(std::isfinite(value));if(f==120){const auto baseline=a.scene.samplePose(*idle,0);for(size_t b=0;b<pose.size();++b)for(int k=0;k<16;++k)CHECK(std::abs(pose[b].v[k]-baseline[b].v[k])<.001f);}if(v<variants.size()&&(f==0||f==90||f==120)&&(game=="bo2"||v==0))capture(pose,"a"+std::to_string(v)+"_f"+std::to_string(f));}
   updateGameplay(a,0);CHECK(!a.actionActive);
  }CHECK(visited.size()==variants.size());
  // Interrupted/repeated attacks start at the previous native-space pose.
  a.gameplayAction=scene::ActionRole::Melee;triggerGameplayAction(a);a.actionElapsed=.3f;a.actionFrame=.3f*a.scene.animations[a.actionAnimationIndex].framerate;auto before=evaluateCurrentPose(a);triggerGameplayAction(a);auto after=evaluateCurrentPose(a);for(size_t b=0;b<before.size();++b)for(int k=0;k<16;++k)CHECK(std::abs(before[b].v[k]-after[b].v[k])<.001f);a.runtimeLayers.clear();a.actionElapsed=authoredClipDuration(a,a.actionAnimationIndex);stopGameplayAction(a);
  const auto bones=a.scene.skeleton.bones.size();captureTakeActorManifest(a);auto manifest=a.recordedTake.actor;CHECK(restoreTakeActor(a,manifest,bones,error));CHECK(a.scene.skeleton.bones.size()==bones&&!a.scene.pointBlankWeaponStem.empty());
  report<<"PASS "<<game<<" / "<<weaponName<<" variants="<<visited.size()<<" finite poses, native timing, idle seam, repeat seam, replay reconstruction\n";report.flush();
 }
 a.renderer.shutdown();state.reset();ImGui::DestroyContext();glfwDestroyWindow(w);glfwTerminate();std::cout<<"32 knife/hand combinations passed\n";return 0;
}
