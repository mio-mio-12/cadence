#include "assets/LocalAssetPaths.h"
#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <fstream>
int main(int argc,char** argv){
 if(!glfwInit())return 1;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(800,600,"Grip audit",nullptr,nullptr);if(!w)return 2;glfwMakeContextCurrent(w);ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
 auto s=std::make_unique<AppState>();auto&a=*s;a.window=w;a.deferSceneUpload=true;std::string error;if(!a.renderer.initialize(error))return 3;
 a.defaultSalukiDirectory=cadence::local_assets::exportPath("");
 const auto out=std::filesystem::path(argc>2?argv[2]:"diagnostics/v158-world-grips");std::filesystem::create_directories(out);std::ofstream report(out/"audit.txt");
 const std::vector<std::string> games={"pointblank","bo2","bo2_sp","mw","mw3","ghosts","aw","iw_sp","mwr","bocw_sp","codm","cs2"};
 for(const auto&g:games){std::cout<<"scan "<<g<<"\n"<<std::flush;(void)assets::appendScan(a.defaultSalukiDirectory/g,g,a.assetCatalog,error);}
 auto pick=[&](const std::string&g,assets::Role role,std::string needle){size_t fallback=SIZE_MAX;for(size_t i=0;i<a.assetCatalog.entries.size();++i){const auto&e=a.assetCatalog.entries[i];if(e.game!=g||e.role!=role)continue;const auto n=lowerText(e.name);if(role==assets::Role::PlayerModel&&(n.find("head")!=std::string::npos||n.find("vest")!=std::string::npos))continue;if(fallback==SIZE_MAX)fallback=i;if(!needle.empty()&&n.find(lowerText(needle))!=std::string::npos)return i;}return fallback;};
 for(const auto&g:games){report<<"GAME "<<g;for(auto role:{assets::Role::PlayerModel,assets::Role::ViewWeapon,assets::Role::WorldWeapon}){size_t count=0;for(auto&e:a.assetCatalog.entries)if(e.game==g&&e.role==role)++count;report<<" "<<assets::roleName(role)<<"="<<count;}report<<"\n";}
 const auto pb=pick("pointblank",assets::Role::PlayerModel,"SWAT_Male_fb");
 const auto python=pick("pointblank",assets::Role::ViewWeapon,"ColtPython"),ak=pick("pointblank",assets::Role::ViewWeapon,"AK-47_DualMag");
 a.autoPlayerModel=false;a.actorMode=a.playing=a.gameplayLogic=true;a.actorPosition={};
 auto run=[&](size_t body,size_t weapon,const std::string&anim,const std::string&label){
  if(argc>1&&label.find(argv[1])==std::string::npos)return;
  if(body==SIZE_MAX||weapon==SIZE_MAX){report<<label<<" UNAVAILABLE\n";return;}
  a.actorCurrLocoClip=a.actorPrevLocoClip=a.actorLastActiveLocoClip=SIZE_MAX;a.actorWorldPresentationTime=-1;a.actorWorldFacingOffset=0;
  a.manualPlayerModelAsset=body;a.selectedWeaponAsset=a.classPrimaryAsset=weapon;a.playerWorldAnimationGame=anim;
  std::cout<<label<<" begin\n"<<std::flush;configureClassActor(a);
  report<<label<<" body="<<a.assetCatalog.entries[body].name<<" weapon="<<a.assetCatalog.entries[weapon].name<<" anim="<<anim<<"\n";
  if(!a.hiddenWorldActor){report<<" NO ACTOR "<<a.status<<"\n";return;}auto&actor=*a.hiddenWorldActor;report<<" attachments="<<actor.attachments.size()<<" clips="<<actor.animations.size()<<"\n";
  for(auto&warning:actor.warnings)report<<" warning="<<warning<<"\n";
  if(actor.attachments.empty())return;
  if(label.starts_with("new_worlds_")){
   const auto world=findWorldWeaponForViewWeapon(a,a.assetCatalog.entries[weapon]);
   if(world==SIZE_MAX)throw std::runtime_error("Missing world counterpart");
   auto native=scene::buildScene(cast::Document::load(a.assetCatalog.entries[world].path));
   std::vector<scene::Mat4> rest;for(const auto& bone:native.skeleton.bones)rest.push_back(bone.restGlobal);
   for(const auto& mesh:native.meshes)report<<" native material="<<mesh.materialName<<" albedo="<<mesh.albedoPath.string()<<'\n';
   if(!a.renderer.loadScene(native,error))throw std::runtime_error(error);
   a.renderer.setDebugView(1);a.renderer.setViewmodelCapture(true,{.13f,.14f,.16f,1});
   const scene::Vec3 center{-20,0,-7},eye{-20,-140,12};a.renderer.setCameraPosition(eye);
   a.renderer.render(native,rest,scene::orthographic(-55,55,-41.25f,41.25f,.1f,1000)*scene::lookAt(eye,center,{0,0,1}),800,600,false,false,false);
   if(!a.renderer.saveColorPng(out/(label+"_native.png"),error))throw std::runtime_error(error);
  }
  for(auto&at:actor.attachments)report<<" mount="<<at.name<<" bone="<<actor.skeleton.bones[at.boneIndex].name<<"\n";
  if(!a.renderer.loadScene(actor,error)){report<<" render error="<<error<<"\n";return;}a.renderer.setDebugView(1);a.renderer.setViewmodelCapture(true,{.13f,.14f,.16f,1});
  size_t wrist=pointBlankWorldGripBone(actor.skeleton);
  if(label=="codm_with_python"||label=="codm_with_pb_ak"){
   std::ifstream input("cadence weapon calibrator reference/latest.json");nlohmann::json refs;input>>refs;
   for(const auto& c:refs["cases"])if(c["case"]==(label=="codm_with_python"?"pistol_python":"rifle_ak47")){
    std::vector<scene::Mat4> referencePose;for(auto&b:actor.skeleton.bones)referencePose.push_back(b.restGlobal);
    for(auto& b:c["frozen_pose"]){auto it=actor.skeleton.boneByName.find(b["name"].get<std::string>());if(it!=actor.skeleton.boneByName.end())for(int k=0;k<16;++k)referencePose[it->second].v[k]=b["global_matrix_column_major"][k];}
    scene::Mat4 vp;for(int k=0;k<16;++k)vp.v[k]=c["camera"]["view_projection_column_major"][k];int width=c["camera"]["viewport_pixels"][0],height=c["camera"]["viewport_pixels"][1];
    a.renderer.render(actor,referencePose,vp,width,height,false,false,false);(void)a.renderer.saveColorPng(out/(label+"_reference.png"),error);
   }
  }
  for(int cue=0;cue<3;++cue){a.weaponProfile=generatedWeaponProfile(a,a.assetCatalog.entries[weapon]);a.gameplayWeapon=gameplayWeaponClass(a.weaponProfile,a.assetCatalog.entries[weapon]);a.actorGrounded=true;a.actorMantling=a.actorSliding=a.actorSprinting=false;a.actionActive=false;a.actorVelocity=a.actorWishVelocity={cue==1?180.f:0.f,0,0};a.actorMoveInputForward=cue==1?1.f:0.f;a.actorMoveInputSide=0;a.gameplayStance=cue==2?scene::Stance::Crouch:scene::Stance::Stand;
   std::string clip;float frame=0;std::vector<scene::Mat4> pose;for(int tick=0;tick<60;++tick){a.gameplayClock+=1.f/60;pose=evaluateHiddenWorldActorPose(a,&clip,&frame,nullptr,nullptr,false);}report<<" cue="<<cue<<" clip="<<clip<<" frame="<<frame;
   if(a.actorCurrLocoClip<actor.animations.size()){const auto& selected=actor.animations[a.actorCurrLocoClip];report<<" sourceGame="<<selected.sourceGame<<" source="<<selected.sourceName;}
   report<<"\n";if(wrist>=pose.size())continue;const auto center=scene::transformPoint(pose[wrist],{});
   for(int side=0;side<2;++side){const auto vp=scene::perspective(45*scene::kPi/180,800.f/600,1,4000)*scene::lookAt(center+(side?scene::Vec3{75,85,28}:scene::Vec3{75,-85,28}),center,{0,0,1});a.renderer.render(actor,pose,vp,800,600,false,false,false);a.renderer.saveColorPng(out/(label+"_"+std::to_string(cue)+"_"+std::to_string(side)+".png"),error);}
   const auto full=scene::perspective(45*scene::kPi/180,800.f/600,1,4000)*scene::lookAt({235,-260,150},{0,0,90},{0,0,1});a.renderer.render(actor,pose,full,800,600,false,false,false);(void)a.renderer.saveColorPng(out/(label+"_"+std::to_string(cue)+"_full.png"),error);
   for(const auto& transform:pose)for(float v:transform.v)if(!std::isfinite(v))throw std::runtime_error("Non-finite world pose: "+label);
  }
  take::ActorManifest manifest;captureCharacterAttachments(a,actor,manifest);manifest.baseModel=a.assetCatalog.entries[body].path.string();
  take::Take saved;saved.boneCount=1;saved.worldBoneCount=static_cast<std::uint32_t>(actor.skeleton.bones.size());saved.worldActor=manifest;
  take::Sample sample;sample.pose={scene::Mat4::identity()};for(const auto& b:actor.skeleton.bones)sample.worldActor.pose.push_back(b.restGlobal);saved.samples.push_back(sample);
  const auto takePath=out/(label+".c_dm");if(!take::save(saved,takePath,error))throw std::runtime_error(error);take::Take reopened;if(!take::load(takePath,reopened,error))throw std::runtime_error(error);manifest=reopened.worldActor;
  scene::CastScene restored;restored.skeleton=actor.skeleton;
  if(!restoreCharacterAttachments(a,manifest,restored,error))report<<" replay attachment FAIL="<<error<<"\n";
  else{std::vector<scene::Vec3> originalPositions,restoredPositions,originalNormals,restoredNormals;for(const auto&m:actor.meshes)if(m.attachmentIndex>=0)for(const auto&v:m.vertices){originalPositions.push_back(v.position);originalNormals.push_back(v.normal);}for(const auto&m:restored.meshes)for(const auto&v:m.vertices){restoredPositions.push_back(v.position);restoredNormals.push_back(v.normal);}if(originalPositions.size()!=restoredPositions.size())throw std::runtime_error("Restored vertex count mismatch");float maxPositionError=0,maxNormalError=0;for(size_t i=0;i<originalPositions.size();++i){maxPositionError=std::max(maxPositionError,scene::length(originalPositions[i]-restoredPositions[i]));maxNormalError=std::max(maxNormalError,scene::length(originalNormals[i]-restoredNormals[i]));}report<<" replay attachment vertices="<<originalPositions.size()<<" maxPositionError="<<maxPositionError<<" maxNormalError="<<maxNormalError<<"\n";if(maxPositionError>1e-4f||maxNormalError>1e-4f)throw std::runtime_error("Restored attachment differs from live geometry");}
  report.flush();
 };
 for(const auto&g:games){if(g=="pointblank"||g=="bo2_sp")continue;const std::string wanted=g=="bo2"?"an94":g=="mw3"?"m4_iw5":g=="ghosts"?"viewmodel_sc2010_LOD0":g=="aw"?"vm_bal27_base_standard_LOD0":g=="bocw_sp"?"sniper_standard_view":g=="cs2"?"ak47":g=="codm"?"pdw":"";run(pb,pick(g,assets::Role::ViewWeapon,wanted),"bo2","pb_with_"+g);auto body=pick(g,assets::Role::PlayerModel,g=="bo2"?"c_usa_mp_isa_smg_fb":"");if(body!=SIZE_MAX){run(body,python,g=="codm"?"bo2":g,g+"_with_python");run(body,ak,g=="codm"?"bo2":g,g+"_with_pb_ak");}}
 run(pb,pick("bo2",assets::Role::ViewWeapon,"an94"),"pointblank","pb_native_with_bo2");
 for(const auto* model:{"sentinel","Seal6_001"}){run(pick("codm",assets::Role::PlayerModel,model),python,"bo2",std::string("codm_")+model+"_python");run(pick("codm",assets::Role::PlayerModel,model),ak,"bo2",std::string("codm_")+model+"_ak");}
 for(const auto* weaponName:{"viewmodel_pistol_DesertEagle","viewmodel_pistol_MK23","viewmodel_AR_TAR-21"})run(pick("codm",assets::Role::PlayerModel,"Charly"),pick("pointblank",assets::Role::ViewWeapon,weaponName),"bo2",std::string("codm_heldout_")+weaponName);
 for(const auto& test:std::vector<std::pair<std::string,std::string>>{{"mw","viewmodel_m40a3_mp_LOD0"},{"mw","viewmodel_ak47_mp_LOD0"},{"mwr","wpn_h1_pst_m9_vm_camo_LOD0"},{"mwr","wpn_h1_smg_ak74u_vm_wet_camo_LOD0"}}){
  const auto weapon=pick(test.first,assets::Role::ViewWeapon,test.second);
  if(weapon==SIZE_MAX||a.assetCatalog.entries[weapon].name!=test.second)throw std::runtime_error("Missing exact representative weapon");
  run(pick("bo2",assets::Role::PlayerModel,"c_usa_mp_isa_smg_fb"),weapon,"bo2","new_worlds_bo2_"+test.second);
  run(pb,weapon,"bo2","new_worlds_pb_"+test.second);
 }
 for(const std::string name:{"viewmodel_kriss_gold_LOD0","viewmodel_lsat_LOD0","viewmodel_magum_iw6_LOD0","viewmodel_vbr_pdw_LOD0","viewmodel_vbr_pdw_gold_LOD0"}){
  const auto weapon=pick("ghosts",assets::Role::ViewWeapon,name);
  if(weapon==SIZE_MAX||a.assetCatalog.entries[weapon].name!=name)throw std::runtime_error("Missing exact Ghosts weapon");
  run(pick("bo2",assets::Role::PlayerModel,"c_usa_mp_isa_smg_fb"),weapon,"bo2","ghost_alias_bo2_"+name);
  run(pb,weapon,"bo2","ghost_alias_pb_"+name);
 }
 a.renderer.shutdown();s.reset();ImGui::DestroyContext();glfwDestroyWindow(w);glfwTerminate();return 0;
}
