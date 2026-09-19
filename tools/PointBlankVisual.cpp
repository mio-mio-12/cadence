#include "scene/PointBlankNative.h"
#include "scene/PointBlankAdapter.h"
#include "render/StageRenderer.h"
#include <GLFW/glfw3.h>
#include <iostream>
int main(int argc,char**argv){
 if(argc<5)return 2;scene::CastScene s;std::string error;
 if(!scene::pointblank::assemble(cast::Document::load(argv[1]),cast::Document::load(argv[2]),s,error)){std::cerr<<error;return 1;}
 const auto clipPath=std::filesystem::path(argv[3]);const auto idlePath=clipPath.parent_path()/(s.pointBlankWeaponStem+"_AttackIdle.cast");
 if(idlePath!=clipPath&&std::filesystem::exists(idlePath)){scene::appendAnimations(cast::Document::load(idlePath),s);s.animations.clear();}
 scene::appendAnimations(cast::Document::load(argv[3]),s);
 if(argc>5){if(std::string(argv[5])=="flip"){for(auto& m:s.meshes)for(auto& v:m.vertices)v.uv.y=1.f-v.uv.y;}else if(std::string(argv[5])!="native"&&!scene::pointblank::fitHands(s,cast::Document::load(argv[2]),cast::Document::load(argv[5]),error)){std::cerr<<error;return 8;}}
 if(s.animations.empty()||s.animations[0].tracks.empty())return 3;
 if(argc>7&&std::string(argv[7])=="bind-owners"){
  auto reference=s;reference.animations.clear();scene::appendAnimations(cast::Document::load(idlePath),reference);auto local=reference.sampleLocalPose(0,0);std::vector<scene::Mat4> bind;
  for(size_t b=0;b<local.size();++b){auto&bone=s.skeleton.bones[b];auto t=bone.name.starts_with("pb2cast_weapon__")&&bone.parent>=0?local[b]:bone.restLocal;bind.push_back(bone.parent>=0?bind[bone.parent]*scene::trs(t.position,t.rotation,t.scale):bone.restGlobal);}
  for(auto&m:s.meshes)if(m.viewmodelWeapon)for(auto&v:m.vertices)for(int k=0;k<4;++k)if(v.weights[k]>.999f&&s.skeleton.bones[v.bones[k]].parent>=0)s.skeleton.bones[v.bones[k]].inverseBind=scene::inverseAffine(bind[v.bones[k]]);
 }
 if(argc>7&&std::string(argv[7])=="bind-leaves"){
  std::vector<bool> leaf(s.skeleton.bones.size(),true),owner(leaf.size(),false);for(auto&b:s.skeleton.bones)if(b.parent>=0)leaf[b.parent]=false;
  for(auto&m:s.meshes)if(m.viewmodelWeapon)for(auto&v:m.vertices)for(int k=0;k<4;++k)if(v.weights[k]>.9f)owner[v.bones[k]]=true;
  std::erase_if(s.animations[0].tracks,[&](const auto&t){return leaf[t.boneIndex]&&owner[t.boneIndex]&&t.property<=scene::TrackProperty::Rotation;});
 }
 if(argc>7&&std::string(argv[7])=="muzzle"){
  const auto it=s.skeleton.boneByName.find("pb2cast_weapon__FXDummy");if(it==s.skeleton.boneByName.end())return 9;
  const auto inv=scene::inverseAffine(s.skeleton.bones[it->second].restGlobal);float furthest=0;int count=0;
  for(const auto&m:s.meshes)if(m.viewmodelWeapon)for(const auto&v:m.vertices){auto p=scene::transformPoint(inv*m.modelTransform,v.position);if(p.x*p.x+p.z*p.z<36&&p.y>=0&&p.y<40){furthest=std::max(furthest,p.y);++count;}}
  std::cout<<"barrel-forward geometry="<<furthest<<" cm candidates="<<count<<"\n";
 }
 std::filesystem::create_directories(argv[4]);
 if(!glfwInit())return 4;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
 auto*w=glfwCreateWindow(64,64,"Point Blank validation",nullptr,nullptr);if(!w)return 5;glfwMakeContextCurrent(w);
 int status=0;{
 render::StageRenderer r;if(!r.initialize(error)||!r.loadScene(s,error)){std::cerr<<error;return 6;}
 r.setViewmodelCapture(true,{.035f,.04f,.045f,1});r.setDebugView(1);
 const int samples=argc>6?std::clamp(std::atoi(argv[6]),5,121):5;
 for(int k=0;k<samples;++k){const auto pose=s.samplePose(0,s.animations[0].durationFrames*k/float(samples-1));
 for(int side=0;side<3;++side){scene::Vec3 eye=side==1?scene::Vec3{180,40,155}:scene::Vec3{0,0,165};scene::Vec3 target=side==1?scene::Vec3{0,40,145}:eye+scene::Vec3{0,1,0};if(side==2){auto hand=s.skeleton.boneByName.at("R Hand"),middle=s.skeleton.boneByName.at("R Middle2");target=(scene::transformPoint(pose[hand],{})+scene::transformPoint(pose[middle],{}))*.5f;}r.setCameraPosition(eye);
 if(argc>7&&std::string(argv[7])=="grip"&&side){auto hand=s.skeleton.boneByName.at("R Hand"),middle=s.skeleton.boneByName.at("R Middle2");target=(scene::transformPoint(pose[hand],{})+scene::transformPoint(pose[middle],{}))*.5f;eye=target+scene::Vec3{side==1?35.f:-35.f,-3,5};r.setCameraPosition(eye);}
 auto vp=scene::perspective(scene::kPi*55/180,16.f/9,.1f,1000)*scene::lookAt(eye,target,{0,0,1});r.render(s,pose,vp,960,540,false,false,false);
 if(argc>7&&std::string(argv[7])=="muzzle"){
  const auto muzzle=scene::resolveMuzzlePosition(s,pose);if(!muzzle){std::cerr<<"Missing barrel socket\n";return 9;}
  r.renderMuzzleFlash3D(*muzzle,3,0,{1,.65f,.1f,1},vp,eye,true);
  r.renderDebugLine3D(*muzzle-scene::Vec3{0,0,4},*muzzle+scene::Vec3{0,0,4},{0,1,0,1},vp);
 }
 if(!r.saveColorPng(std::filesystem::path(argv[4])/(std::string(side==2?"close_":side?"side_":"view_")+std::to_string(k)+".png"),error)){std::cerr<<error;status=7;}
 }}
 }glfwDestroyWindow(w);glfwTerminate();return status;
}
