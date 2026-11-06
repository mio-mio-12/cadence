#include "scene/CastScene.h"
#include "render/StageRenderer.h"
#include "scene/ImportedViewRetarget.h"
#include <GLFW/glfw3.h>
#include <iostream>
int main(int argc,char** argv){
 try {
 if(argc!=4&&argc!=7)return 2;const std::filesystem::path root=argv[1],out=argv[3];std::filesystem::create_directories(out);
 const auto sourceDoc=cast::Document::load(argc==7?std::filesystem::path(argv[4]):root/"cs2/models/viewhands/viewhands_model.cast");
 const auto targetDoc=cast::Document::load(argv[2]);auto source=scene::buildScene(sourceDoc),target=scene::buildScene(targetDoc);
 const auto gun=cast::Document::load(argc==7?std::filesystem::path(argv[5]):root/"cs2/models/weapons/knife_css/knife_css_model.cast");
 const auto clip=cast::Document::load(argc==7?std::filesystem::path(argv[6]):root/"cs2/animations/viewmodel/knife_css/draw_css.cast");
 if(!sourceDoc.valid()||!targetDoc.valid()||!gun.valid()||!clip.valid())return 3;
 if(argc==7){auto driver=scene::buildScene(gun);std::string error;for(auto& m:driver.meshes)m.viewmodelWeapon=true;
  std::cout<<"paths "<<driver.sourceModelPath<<" "<<source.sourceModelPath<<'\n';
  for(const auto& b:source.skeleton.bones){auto it=driver.skeleton.boneByName.find(b.name);if(it==driver.skeleton.boneByName.end()){std::cout<<"missing "<<b.name<<'\n';continue;}float delta=0;for(int n=0;n<16;++n)delta=std::max(delta,std::abs(b.restGlobal.v[n]-driver.skeleton.bones[it->second].restGlobal.v[n]));if(delta>.002f)std::cout<<"bind delta "<<b.name<<" "<<delta<<'\n';}
  if(!scene::imported::fitPoseOnlyViewSkin(driver,source,error)){std::cerr<<error;return 9;}source=std::move(driver);
 }else scene::appendRigModel(gun,source,"knife",scene::Mat4::identity(),1);
 const auto scale=scene::source2RetargetScale(target.skeleton,source.skeleton);
 if(argc==7){auto driver=scene::buildScene(gun);std::string error;for(auto& m:driver.meshes)m.viewmodelWeapon=true;
  if(!scene::imported::fitPoseOnlyViewSkin(driver,target,error)){std::cerr<<error;return 9;}target=std::move(driver);
 }else scene::appendRigModel(gun,target,"knife",scene::Mat4::identity(),scale);
 scene::appendAnimations(clip,source);scene::appendAnimations(clip,target);
 if(argc!=7&&!scene::retargetSource2AnimationRange(target,0,source,0))return 4;
 source.animations[0].looping=target.animations[0].looping=false;
 for(const auto* s:{&source,&target}){const auto pose=s->samplePose(0,15);
  for(std::size_t i=0;i<s->skeleton.bones.size();++i){const auto& b=s->skeleton.bones[i];
   if(b.name=="armUpperShoulder_R"||b.name=="arm_upper_R"||b.name=="arm_lower_R"||b.name=="hand_R"||b.name=="j_shoulder_ri"||b.name=="j_elbow_ri"||b.name=="j_wrist_ri")
    std::cout<<b.name<<" rest "<<b.restGlobal.v[12]<<','<<b.restGlobal.v[13]<<','<<b.restGlobal.v[14]<<" pose "<<pose[i].v[12]<<','<<pose[i].v[13]<<','<<pose[i].v[14]<<'\n';
  }
 }
 for(const auto& warning:source.warnings)std::cout<<warning<<'\n';
 if(!glfwInit())return 5;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(960,540,"Sleeve audit",nullptr,nullptr);if(!window)return 5;glfwMakeContextCurrent(window);
 render::StageRenderer renderer;std::string error;if(!renderer.initialize(error))return 6;
 for(int variant=0;variant<2;++variant){const auto& s=variant?target:source;
  if(!renderer.loadScene(s,error))return 7;renderer.setDebugView(1);renderer.setViewmodelCapture(true,{.08f,.09f,.11f,1});
  scene::Vec3 eye{};if(variant&&s.animations[0].viewmodelCameraReference)eye=scene::transformPoint(*s.animations[0].viewmodelCameraReference,{});
  const auto vp=scene::perspective(75*scene::kPi/180,960.f/540,.02f,5000)*scene::lookAtDirection(eye,{1,0,0},{0,0,1});renderer.setCameraPosition(eye);
  for(int frame:{0,7,15,23,30}){auto pose=s.samplePose(0,float(frame));
   renderer.render(s,pose,vp,960,540,false,false,false);
   if(!renderer.saveColorPng(out/(std::string(variant?"mwr_":"native_")+std::to_string(frame)+".png"),error))return 8;
  }
 }
 renderer.shutdown();glfwDestroyWindow(window);glfwTerminate();std::cout<<"Sleeve captures saved\n";return 0;
 }catch(const std::exception& e){std::cerr<<"Audit failed: "<<e.what()<<'\n';return 10;}
}
