#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <fstream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<" "<<a.status<<"\n";return 1;}}while(false)
int main(){
 if(!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(960,540,"v142",nullptr,nullptr);if(!w)return 3;glfwMakeContextCurrent(w);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
 auto state=std::make_unique<AppState>();auto&a=*state;a.window=w;a.deferSceneUpload=true;std::string error;CHECK(a.renderer.initialize(error));a.defaultSalukiDirectory="D:/Editing/COD Resource/3D Rip/saluki/exported_files";
 for(auto g:{"bo2","codm","pointblank"})CHECK(assets::appendScan(a.defaultSalukiDirectory/g,g,a.assetCatalog,error));
 auto idx=[&](const std::string& n){for(size_t i=0;i<a.assetCatalog.entries.size();++i)if(a.assetCatalog.entries[i].name==n)return i;return SIZE_MAX;};
 const auto out=std::filesystem::path("diagnostics/v142");std::filesystem::create_directories(out);std::ofstream report(out/"checks.txt");
 a.selectedBaseAsset=idx("c_usa_mp_isa_smg_viewhands_LOD0");CHECK(a.selectedBaseAsset!=SIZE_MAX);auto weapon=idx("t6_wpn_smg_pdw57_view_LOD0");if(weapon==SIZE_MAX)weapon=idx("t6_wpn_ar_an94_view_LOD0");CHECK(weapon!=SIZE_MAX);equipViewWeapon(a,weapon);auto idle=findViewmodelClip(a,"idle");CHECK(idle);a.animationIndex=*idle;a.animationFrame=0;
 const auto base=a.scene.samplePose(*idle,0);const auto clip=a.defaultSalukiDirectory/"codm/animations/viewmodel_smg_pdw57_Default_inspection_s.cast";
 CHECK(matchesAnimationImportAction(clip,"inspect"));CHECK(!matchesAnimationImportAction(clip,"reload"));
 CHECK(matchesAnimationImportAction("viewmodel_pistol_Test_JumpStart_AttackIdle.cast","jump_takeoff"));CHECK(!matchesAnimationImportAction("viewmodel_pistol_Test_JumpStart_AttackIdle.cast","idle"));
 CHECK(matchesAnimationImportAction("viewmodel_sniper_Test_ReloadC.cast","rechamber"));CHECK(!matchesAnimationImportAction("viewmodel_sniper_Test_ReloadC.cast","reload"));
 queueAnimationFolders(a,{a.defaultSalukiDirectory/"codm/animations"},"ads_down");CHECK(!a.pendingAnimationFiles.empty());for(const auto&p:a.pendingAnimationFiles)CHECK(matchesAnimationImportAction(p,"ads_down"));report<<"Scoped ADS-down queue "<<a.pendingAnimationFiles.size()<<" files\n";a.pendingAnimationFiles.clear();a.loadingActive=false;
 const auto first=a.scene.animations.size();addAnimationFile(a,clip);CHECK(a.scene.animations.size()>first);CHECK(a.scene.animations[first].coldWarWorldPose);CHECK(!a.scene.animations[first].tracks.empty());
 // Calibration at the native idle must reproduce the recipient reference, not rotate it.
 auto calibrated=std::make_shared<scene::ColdWarWorldPose>(*a.scene.animations[first].coldWarWorldPose);calibrated->animation=0;const auto reference=a.scene.globalPose(calibrated->sample(a.scene.skeleton,0));float err=0;for(size_t i=0;i<base.size();++i)for(int k=0;k<16;++k)err=std::max(err,std::abs(base[i].v[k]-reference[i].v[k]));CHECK(err<.01f);report<<"CODM idle calibration max error "<<err<<"\n";
 CHECK(a.renderer.loadScene(a.scene,error));a.renderer.setViewmodelCapture(true,{.04f,.04f,.05f,1});a.renderer.setDebugView(1);
 for(int f=0;f<=4;++f){const auto p=a.scene.samplePose(first,a.scene.animations[first].durationFrames*f/4.f);for(auto&m:p)for(float v:m.v)CHECK(std::isfinite(v));const auto c=p[a.scene.skeleton.boneByCanonicalName.at("tag_camera")];scene::Vec3 eye{c.v[12],c.v[13],c.v[14]},forward{c.v[0],c.v[1],c.v[2]},up{c.v[8],c.v[9],c.v[10]};a.renderer.render(a.scene,p,scene::perspective(65*scene::kPi/180,16.f/9,.1f,2000)*scene::lookAtDirection(eye,forward,up),960,540,false,false,false);CHECK(a.renderer.saveColorPng(out/("codm_inspect_"+std::to_string(f)+".png"),error));}
 a.weaponProfile.animations["inspect"]=clip.filename().string();captureWeaponRigState(a);CHECK(a.weaponProfile.animationFiles.contains(clip.filename().string()));CHECK(weapon::save(a.weaponProfile,out/"foreign.iwweapon",error));weapon::Profile saved;CHECK(weapon::load(out/"foreign.iwweapon",saved,error));CHECK(saved.animationFiles==a.weaponProfile.animationFiles);a.scene.animations.resize(first);a.animationSources.clear();a.animationDocuments.clear();auto catalog=a.assetCatalog;std::erase_if(a.assetCatalog.entries,[](const auto& asset){return asset.game=="codm";});activateWeaponProfile(a,saved);a.assetCatalog=std::move(catalog);CHECK(a.scene.animations.size()>first&&a.scene.animations.back().coldWarWorldPose);report<<"Exact foreign dependency reload with CODM unselected passed\n";
 for(const auto* model:{"playermode_SWAT_Male_fb","playermode_SWAT_Female_SWAT-Sniper_fb","playermode_SWAT_Male_Madness_fb"}){
  const auto m=idx(model);CHECK(m!=SIZE_MAX);scene::CastScene actor=scene::buildScene(cast::Document::load(a.assetCatalog.entries[m].path),false);appendClassBodyAnimations(a,"bo2",actor);CHECK(!actor.animations.empty());CHECK(a.renderer.loadScene(actor,error));
  for(auto stance:{scene::Stance::Stand,scene::Stance::Crouch,scene::Stance::Prone})for(auto motion:{scene::MotionRole::Idle,scene::MotionRole::Run,scene::MotionRole::Sprint}){
   scene::AnimationQuery q;q.domain=scene::AnimationDomain::PlayerBody;q.stance=stance;q.motion=motion;q.weapon=scene::WeaponClass::Rifle;
   auto found=scene::findBestAnimation(actor,q);if(!found)continue;const auto c=*found;report<<model<<" "<<static_cast<int>(stance)<<" "<<static_cast<int>(motion)<<" -> "<<actor.animations[c].sourceName<<"\n";
   for(int f=0;f<3;++f){auto p=actor.samplePose(c,actor.animations[c].durationFrames*f/2.f);for(auto&m:p)for(float v:m.v)CHECK(std::isfinite(v));const auto root=actor.skeleton.boneByCanonicalName.at("tag_origin");const auto delta=scene::transformPoint(p[root],{})-scene::transformPoint(actor.skeleton.bones[root].restGlobal,{});for(auto& b:p){b.v[12]-=delta.x;b.v[13]-=delta.y;b.v[14]-=delta.z;}const auto vp=scene::perspective(45*scene::kPi/180,16.f/9,1.f,2000.f)*scene::lookAt({230,-270,135},{0,0,80},{0,0,1});a.renderer.render(actor,p,vp,960,540,false,false,false);CHECK(a.renderer.saveColorPng(out/(std::string(model)+"_"+std::to_string(static_cast<int>(stance))+"_"+std::to_string(static_cast<int>(motion))+"_"+std::to_string(f)+".png"),error));}
  }
 }
 a.renderer.shutdown();state.reset();ImGui::DestroyContext();glfwDestroyWindow(w);glfwTerminate();std::cout<<"v142 checks passed\n";
}
