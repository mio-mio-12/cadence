#include "assets/LocalAssetPaths.h"
#define main cadenceProductionMain
#include "../src/app/main.cpp"
#undef main
#include <iostream>
int main(int argc,char** argv) try {
 const auto output=std::filesystem::absolute(argc>1?argv[1]:"diagnostics/bot-playback-audit");
 if(!glfwInit())return 1;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto*w=glfwCreateWindow(800,600,"Bot playback audit",nullptr,nullptr);glfwMakeContextCurrent(w);
 ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui_ImplGlfw_InitForOpenGL(w,true);ImGui_ImplOpenGL3_Init("#version 330");
 auto state=std::make_unique<AppState>();auto& a=*state;a.window=w;a.defaultSalukiDirectory=cadence::local_assets::exportPath("");std::string error;
 if(!a.renderer.initialize(error))return 2;
 for(auto g:{"bo2","mw3","pointblank"})if(!assets::appendScan(a.defaultSalukiDirectory/g,g,a.assetCatalog,error))return 3;
 const bool fixedPb=argc>2&&std::string(argv[2])=="pb-fixed";
 if(fixedPb){const std::set<std::string> names={"playermode_Bella_fb","playermode_Hide_Recon_fb","playermode_Hide_Soccer_fb","playermode_Hide_Kopassus_fb","playermode_REBEL_Soccer_fb"};std::erase_if(a.assetCatalog.entries,[&](const auto& entry){return entry.game=="pointblank"&&entry.name.starts_with("playermode_")&&!names.contains(entry.name);});}
 for(size_t i=0;i<a.assetCatalog.entries.size();++i)if(a.assetCatalog.entries[i].name=="t6_wpn_ar_an94_view_LOD0")a.selectedWeaponAsset=i;
 a.enemyBotCount=5;a.botTeamSide=3;a.botAnimationGame="bo2+bo2_sp";a.playing=true;a.actorPosition={0,0,0};a.botSystemMode=1;a.showBotAnimationClips=true;
 std::filesystem::create_directories(output);std::ofstream log(output/"results.txt");
 for(auto g:{"bo2","pointblank","mw3"}){
  if(fixedPb&&std::string(g)!="pointblank")continue;
  a.botGame=g;rebuildBotActors(a);log<<"BUILD "<<g<<" "<<a.status<<std::endl;if(!a.botActorScene)return 4;
  auto&s=*a.botActorScene;log<<"BONES "<<s.skeleton.bones.size()<<" CLIPS "<<s.animations.size()<<std::endl;
  for(int weapon=0;weapon<=int(scene::WeaponClass::RC);++weapon)for(auto m:{scene::MotionRole::Idle,scene::MotionRole::Walk,scene::MotionRole::Run,scene::MotionRole::Sprint}){
   scene::AnimationQuery q;q.domain=scene::AnimationDomain::PlayerBody;q.motion=m;q.stance=scene::Stance::Stand;q.weapon=scene::WeaponClass(weapon);q.direction=scene::Direction::Forward;
   auto c=botLocomotionAnimation(s,q,&a);if(!c){log<<"MISSING "<<int(m)<<std::endl;continue;}const auto&clip=s.animations[*c];auto p=s.samplePose(*c,0),r=s.samplePose(*c,clip.durationFrames*.4f);float difference=0;
   for(size_t b=0;b<p.size();++b)difference=std::max(difference,scene::length(scene::transformPoint(p[b],{})-scene::transformPoint(r[b],{})));
   log<<"SELECT "<<weapon<<" "<<int(m)<<" "<<clip.sourceName<<" frames="<<clip.durationFrames<<" looping="<<clip.looping<<" motion="<<difference<<std::endl;
   if(!isGroundWorldMotion(clip.motion)||clip.contextual||clip.action!=scene::ActionRole::None)return 5;
  }
  const scene::WeaponClass classes[]={scene::WeaponClass::Rifle,scene::WeaponClass::Knife,scene::WeaponClass::DualWield,scene::WeaponClass::Equipment,scene::WeaponClass::Any};
  for(size_t i=0;i<a.bots.size();++i)a.bots[i].weaponClass=classes[i%5];
  // Preserve the production evaluated matrices, then verify disk round-trip.
  take::Take recorded;recorded.sampleRate=60;recorded.boneCount=1;recorded.botBoneCount=static_cast<unsigned>(s.skeleton.bones.size());recorded.botCount=5;
  captureTakeBotManifest(a);recorded.botActor=a.recordedTake.botActor;
  for(int f=0;f<360;++f){updateBotActors(a,1.f/60);if(f%60==0){for(size_t i=0;i<a.bots.size();++i){const auto&b=a.bots[i];log<<"LIVE "<<f<<" "<<i<<" clip="<<b.animation<<" frame="<<b.animationFrame<<" speed="<<gameplay::bot::horizontalSpeed(b)<<" ground="<<b.grounded<<"\n"<<a.botAnimationLabels[i]<<std::endl;}}
   take::Sample sample;sample.time=f/60.f;sample.pose={scene::Mat4::identity()};
   for(size_t i=0;i<a.bots.size();++i){const auto&b=a.bots[i];take::RecordedActorState state;state.id=b.id;state.modelVariant=b.modelVariant;state.pose=a.botActorPoses[i];state.primaryClip=s.animations[b.animation].sourceName;state.primaryFrame=b.animationFrame;sample.bots.push_back(std::move(state));}
   recorded.samples.push_back(std::move(sample));
   if(f==65||f==80){
    auto poses=a.botActorPoses;std::vector<int> variants;
    for(size_t i=0;i<poses.size();++i){variants.push_back(a.bots[i].modelVariant);auto p=gameplay::bot::presentationPosition(a.bots[i]);for(auto&m:poses[i]){m.v[12]+=float(i)*120-p.x;m.v[13]-=p.y;m.v[14]-=p.z;}}
    scene::CastScene empty;const scene::Vec3 eye{240,-650,230};a.renderer.setCameraPosition(eye);a.renderer.setDebugView(1);
    auto vp=scene::perspective(60*scene::kPi/180,4.f/3,1,4000)*scene::lookAt(eye,{240,0,80},{0,0,1});
    a.renderer.render(empty,{},vp,1000,750,false,false,false,&s,&poses,&variants,nullptr,nullptr,false);
    if(!a.renderer.saveColorPng(output/(std::string(g)+"_"+std::to_string(f)+".png"),error))return 10;
   }
  }
  const auto path=output/(std::string(g)+".c_dm");take::Take loaded;
  if(!take::save(recorded,path,error)||!take::load(path,loaded,error)){log<<error;return 6;}
  for(size_t f=0;f<recorded.samples.size();++f)for(size_t i=0;i<5;++i){const auto&before=recorded.samples[f].bots[i];const auto&after=loaded.samples[f].bots[i];if(before.primaryClip!=after.primaryClip||before.modelVariant!=after.modelVariant)return 7;for(size_t b=0;b<before.pose.size();++b)for(int k=0;k<16;++k)if(std::abs(before.pose[b].v[k]-after.pose[b].v[k])>.001f)return 8;}
  const auto fractional=loaded.interpolatedSample(1.075f,{nullptr,nullptr,&s.skeleton});
  for(const auto&bot:fractional.bots)for(const auto&m:bot.pose)for(float v:m.v)if(!std::isfinite(v))return 9;
  log<<"REPLAY exact recorded poses/clip identities and finite fractional playback PASS"<<std::endl;
  // Recreate geometry/skeleton from the saved manifest, not just matrices.
  // Identical pose arrays can still render stretched on a mismatched rig.
  if(argc>2&&std::string(argv[2])=="skip-restore")continue;
  log<<"RESTORE copying source geometry"<<std::endl;
  const auto sourceSkeleton=s.skeleton;const auto sourceMeshes=s.meshes;
  log<<"RESTORE invoking production reconstruction"<<std::endl;
  if(!restoreTakeBotActor(a,loaded.botActor,loaded.botBoneCount,error)||!a.botActorScene){log<<"RESTORE FAILED "<<error<<std::endl;return 11;}
  const auto& rebuilt=*a.botActorScene;
  if(rebuilt.skeleton.bones.size()!=sourceSkeleton.bones.size()||rebuilt.meshes.size()!=sourceMeshes.size()){log<<"RESTORE layout mismatch"<<std::endl;return 12;}
  for(size_t b=0;b<sourceSkeleton.bones.size();++b){
   if(rebuilt.skeleton.bones[b].name!=sourceSkeleton.bones[b].name)return 13;
   for(int k=0;k<16;++k)if(std::abs(rebuilt.skeleton.bones[b].inverseBind.v[k]-sourceSkeleton.bones[b].inverseBind.v[k])>.001f)return 14;
  }
  // Live assembly interleaves weapons with bodies; restoration appends weapons last.
  // Compare a one-to-one geometry multiset rather than incidental draw order.
  std::vector<bool> matched(rebuilt.meshes.size());
  for(const auto& before:sourceMeshes){bool found=false;
   for(size_t m=0;m<rebuilt.meshes.size()&&!found;++m){const auto& after=rebuilt.meshes[m];
    if(matched[m]||before.actorVariant!=after.actorVariant||before.vertices.size()!=after.vertices.size()||before.indices!=after.indices)continue;
    bool equal=true;for(size_t v=0;v<before.vertices.size()&&equal;++v)equal=scene::length(before.vertices[v].position-after.vertices[v].position)<=.001f&&before.vertices[v].bones==after.vertices[v].bones&&before.vertices[v].weights==after.vertices[v].weights;
    if(equal){matched[m]=true;found=true;}
   }
   if(!found){log<<"RESTORE no matching geometry: "<<before.name<<" variant "<<before.actorVariant<<std::endl;return 15;}
  }
  log<<"REPLAY restored manifest geometry, variant assignment, bone order, weights and inverse binds PASS"<<std::endl;
 }
 return 0;
}catch(const std::exception& e){std::cerr<<"AUDIT exception: "<<e.what()<<std::endl;return 20;}
