#include "scene/PointBlankNative.h"
#include <iostream>
#include <filesystem>
int main(){
 const std::filesystem::path root="D:/Editing/COD Resource/3D Rip/saluki/exported_files";
 auto pb=scene::buildScene(cast::Document::load(root/"pointblank/models/playermodels/SWAT/playermode_SWAT_Male_fb/playermode_SWAT_Male_fb.cast"));
 for(auto name:{"Root","Pelvis","R Hand"}){auto b=pb.skeleton.boneByName.at(name);std::cout<<name<<" bind="<<pb.skeleton.bones[b].restGlobal.v[14]<<"\n";}
 for(auto&entry:std::filesystem::recursive_directory_iterator(root/"pointblank/animations/pb")){
  auto n=entry.path().filename().string();if(n.find("SWAT_Male") == std::string::npos||n.find("Death")==std::string::npos||n.find("death-scythe")!=std::string::npos||entry.path().extension()!=".cast")continue;
  auto a=pb;scene::appendAnimations(cast::Document::load(entry.path()),a);if(a.animations.empty())continue;
  std::cout<<n;for(float f:{0.f,float(a.animations[0].durationFrames)}){auto pose=a.samplePose(0,f);float lo=1e9f;for(auto&m:a.meshes)for(auto&v:m.vertices){scene::Vec3 p{};for(int k=0;k<4;++k)if(v.weights[k]>0)p+=scene::transformPoint(pose[v.bones[k]]*a.skeleton.bones[v.bones[k]].inverseBind,v.position)*v.weights[k];lo=std::min(lo,p.z);}std::cout<<" frame="<<f<<" root="<<pose[a.skeleton.boneByName.at("Root")].v[14]<<" pelvis="<<pose[a.skeleton.boneByName.at("Pelvis")].v[14]<<" floor="<<lo;}std::cout<<"\n";
 }
}
