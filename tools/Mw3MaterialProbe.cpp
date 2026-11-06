#include "cast/CastDocument.h"
#include "scene/CastScene.h"
#include "render/StageRenderer.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <algorithm>
int main(int argc,char** argv){
 if(argc!=3)return 2;const std::filesystem::path root=argv[1],out=argv[2];std::filesystem::create_directories(out);
 if(!glfwInit())return 3;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
 auto* w=glfwCreateWindow(960,640,"Material check",nullptr,nullptr);if(!w)return 4;glfwMakeContextCurrent(w);
 int failures=0;
 {render::StageRenderer renderer;std::string error;if(!renderer.initialize(error)){std::cerr<<error;return 5;}
 renderer.setDebugView(1);
 for(const auto* file:{"mw3/models/weapons/view/rifles/viewmodel_ak47_iw5/viewmodel_ak47_iw5_LOD0.cast","mw3/models/weapons/view/snipers/viewmodel_as50_iw5/viewmodel_as50_iw5_LOD0.cast"}){
  auto s=scene::buildScene(cast::Document::load(root/file));if(s.meshes.empty()){++failures;continue;}
  for(const auto& m:s.meshes){
   std::cout<<m.materialName<<" cutout="<<m.alphaTest<<" blend="<<m.forceAlpha<<" albedo="<<m.albedoPath.string()<<'\n';
   if(m.alphaTest||m.albedoPath.empty()||!std::filesystem::is_regular_file(m.albedoPath))++failures;
  }
  const auto center=(s.bounds.minimum+s.bounds.maximum)*.5f;const auto ext=s.bounds.maximum-s.bounds.minimum;
  const float radius=std::max({ext.x,ext.y,ext.z,1.f})*.65f;
  const auto eye=center+scene::normalize(scene::Vec3{.2f,-1.f,.35f})*radius*3.f;
  const auto vp=scene::orthographic(-radius*1.5f,radius*1.5f,-radius,radius,.01f,radius*8)*scene::lookAt(eye,center,{0,0,1});
  renderer.setCameraPosition(eye);
  std::vector<scene::Mat4> pose;for(const auto& b:s.skeleton.bones)pose.push_back(b.restGlobal);
  for(bool old:{false,true}){
   auto sample=s;if(old)for(auto& m:sample.meshes){m.alphaTest=true;m.camoBlend=false;}
   if(!renderer.loadScene(sample,error)){std::cerr<<error;++failures;continue;}
   renderer.render(sample,pose,vp,960,640,false,false,false);glFinish();
   if(!renderer.saveColorPng(out/(std::filesystem::path(file).stem().string()+(old?"_old_alpha.png":"_fixed.png")),error)){std::cerr<<error;++failures;}
  }
 }
 auto rig=scene::buildScene(cast::Document::load(root/"bo2/models/viewhands/seal6/c_usa_mp_seal6_longsleeve_viewhands/c_usa_mp_seal6_longsleeve_viewhands_LOD0.cast"));
 scene::appendRigModel(cast::Document::load(root/"bo2/models/weapons/view/pistols/t6_wpn_pistol_fnp45_view/t6_wpn_pistol_fnp45_view_LOD0.cast"),rig,"fnp45");
 scene::appendRigModel(cast::Document::load(root/"ghosts/models/weapons/view/melee/viewmodel_knife_iw6/viewmodel_knife_iw6_LOD0.cast"),rig,"iw6_auxiliary",std::nullopt,1.f,true);
 const auto knifeRoot=rig.rigParts.back().rootBones.front();
 scene::appendAnimations(cast::Document::load(root/"bo2/animations/viewmodel/m4m203/viewmodel_m4m203_knife_melee_1.cast"),rig);
 if(rig.animations.empty())++failures;
 else for(bool visible:{false,true})for(float phase:{.25f,.5f}){
  if(visible)rig.hiddenBones.erase(knifeRoot);else rig.hiddenBones.insert(knifeRoot);
  const auto pose=rig.samplePose(0,rig.animations[0].durationFrames*phase);
  scene::Vec3 lo{1e9f,1e9f,1e9f},hi{-1e9f,-1e9f,-1e9f};
  for(const auto& mesh:rig.meshes)for(const auto& v:mesh.vertices){scene::Vec3 p{};
   for(int i=0;i<4;++i)if(v.weights[i]>0&&v.bones[i]<pose.size())p+=scene::transformPoint(pose[v.bones[i]]*rig.skeleton.bones[v.bones[i]].inverseBind,v.position)*v.weights[i];
   lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};
  }
  const auto center=(lo+hi)*.5f;const auto extent=hi-lo;const float radius=std::max({extent.x,extent.y,extent.z,1.f})*.65f;
  const auto eye=center+scene::normalize(scene::Vec3{.3f,-1,.5f})*radius*3;
  renderer.setCameraPosition(eye);if(!renderer.loadScene(rig,error)){++failures;continue;}
  renderer.render(rig,pose,scene::orthographic(-radius*1.5f,radius*1.5f,-radius,radius,.01f,radius*8)*scene::lookAt(eye,center,{0,0,1}),960,640,false,false,false);
  if(!renderer.saveColorPng(out/(std::string("iw6_knife_")+(visible?"visible_":"hidden_")+std::to_string(int(phase*100))+".png"),error))++failures;
 }
 renderer.shutdown();}
 glfwDestroyWindow(w);glfwTerminate();std::cout<<"Material failures: "<<failures<<'\n';return failures?1:0;
}
