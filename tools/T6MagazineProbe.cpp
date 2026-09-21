#include "cast/CastDocument.h"
#include "scene/CastScene.h"
#include "scene/T6Magazine.h"
#include <filesystem>
#include <iostream>
#include <map>
#include <algorithm>
#include <cmath>

bool solve(double a[6][7],double x[6]){
 for(int k=0;k<6;k++){int p=k;for(int i=k+1;i<6;i++)if(std::abs(a[i][k])>std::abs(a[p][k]))p=i;if(std::abs(a[p][k])<1e-5)return false;for(int j=k;j<7;j++)std::swap(a[k][j],a[p][j]);double v=a[k][k];for(int j=k;j<7;j++)a[k][j]/=v;for(int i=0;i<6;i++)if(i!=k){v=a[i][k];for(int j=k;j<7;j++)a[i][j]-=v*a[k][j];}}for(int i=0;i<6;i++)x[i]=a[i][6];return true;
}
void fits(const scene::CastScene& rig,size_t ai){
 const auto mag=rig.skeleton.boneByCanonicalName.find("tag_clip");if(mag==rig.skeleton.boneByCanonicalName.end())return;
 const auto gun=rig.skeleton.boneByCanonicalName.at("j_gun");
 for(const char* name:{"j_wrist_le","j_wrist_ri"}){
  const auto w=rig.skeleton.boneByCanonicalName.at(name);std::vector<scene::Mat4> wrists,mags;
  for(unsigned f=0;f<=rig.animations[ai].durationFrames;f++){auto p=rig.samplePose(ai,float(f));auto inv=scene::inverseAffine(p[gun]);wrists.push_back(inv*p[w]);mags.push_back(inv*p[mag->second]);}
  for(const int window:{16,12,10}){
  struct Fit{double error;int begin;double x[6];};std::vector<Fit> results;
  for(int begin=0;begin+window<int(wrists.size());begin++){
   if(scene::length(scene::transformPoint(mags[begin],{})-scene::transformPoint(mags[begin+window-1],{}))<3)continue;
   double a[6][7]{},x[6]{};for(int f=begin;f<begin+window;f++)for(int k=0;k<3;k++){double row[6]{};row[k]=1;for(int j=0;j<3;j++)row[3+j]=-wrists[f].v[j*4+k];double y=wrists[f].v[12+k]-mags[f].v[12+k];for(int i=0;i<6;i++){for(int j=0;j<6;j++)a[i][j]+=row[i]*row[j];a[i][6]+=row[i]*y;}}
   if(!solve(a,x))continue;double e=0;for(int f=begin;f<begin+window;f++)for(int k=0;k<3;k++){double d=mags[f].v[12+k]+x[k]-wrists[f].v[12+k];for(int j=0;j<3;j++)d-=wrists[f].v[j*4+k]*x[3+j];e+=d*d;}Fit r{sqrt(e/window),begin,{}};std::copy(x,x+6,r.x);results.push_back(r);
  }
  std::sort(results.begin(),results.end(),[](auto&a,auto&b){return a.error<b.error;});
  for(size_t i=0;i<std::min(size_t(5),results.size());i++){const auto&r=results[i];auto mount=rig.skeleton.bones[mag->second].restLocal.position;std::cout<<"FIT window="<<window<<" "<<rig.animations[ai].name<<" "<<name<<" "<<r.begin<<" rms="<<r.error<<" mount="<<mount.x+r.x[0]<<","<<mount.y+r.x[1]<<","<<mount.z+r.x[2]<<" grip="<<r.x[3]<<","<<r.x[4]<<","<<r.x[5]<<"\n";}
  }
 }
}

int main(int argc,char**argv){
 if(argc<2)return 1;
 std::map<std::string,std::filesystem::path> files;
 for(const auto& f:std::filesystem::recursive_directory_iterator(argv[1]))if(f.path().extension()==".cast")files.try_emplace(f.path().stem().string(),f.path());
 const auto find=[&](const std::string& s){auto i=files.find(s);return i==files.end()?std::filesystem::path{}:i->second;};
 const auto v=[](scene::Vec3 p){std::cout<<p.x<<","<<p.y<<","<<p.z;};
 std::vector<std::string> weapons={"ar_an94","ar_hk416","smg_mp7","sniper_dsr50","sniper_ballista","ar_scarh"};if(argc>2)weapons.assign(argv+2,argv+argc);
 for(const auto& weapon:weapons){
  std::string key=weapon;key=key.substr(key.find('_')+1);const std::string name="t6_wpn_"+std::string(weapon)+"_view_LOD0";
  const auto path=find(name);if(path.empty())continue;
  auto raw=scene::buildScene(cast::Document::load(path));std::cout<<"\nWEAPON "<<name<<"\n";
  const auto worldPath=find("t6_wpn_"+std::string(weapon)+"_world_LOD0");if(!worldPath.empty()){
   const auto world=scene::buildScene(cast::Document::load(worldPath));std::cout<<"WORLD\n";
   for(const auto& b:world.skeleton.bones){std::cout<<b.name<<" parent="<<b.parent<<" local=";v(b.restLocal.position);std::cout<<"\n";}
  }
  for(size_t i=0;i<raw.skeleton.bones.size();++i){const auto& b=raw.skeleton.bones[i];std::cout<<i<<" "<<b.name<<" parent="<<b.parent<<" local=";v(b.restLocal.position);std::cout<<" global=";v(scene::transformPoint(b.restGlobal,{}));std::cout<<"\n";}
  for(const auto& part:{name,"t6_attach_mag_"+key+"_view_LOD0","t6_attach_fastmag_"+key+"_view_LOD0"}){
   const auto p=find(part);if(p.empty())continue;const auto m=scene::buildScene(cast::Document::load(p));std::cout<<"PART "<<part<<"\n";
   for(size_t b=0;b<m.skeleton.bones.size();++b){if(m.skeleton.bones[b].name.find("clip")==std::string::npos)continue;scene::Bounds bounds;size_t n=0;scene::Vec3 sum{};
    for(const auto& mesh:m.meshes)for(const auto& x:mesh.vertices)for(size_t j=0;j<x.bones.size();++j)if(x.bones[j]==b&&x.weights[j]>.5f){if(!n)bounds.minimum=bounds.maximum=x.position;else{bounds.minimum={std::min(bounds.minimum.x,x.position.x),std::min(bounds.minimum.y,x.position.y),std::min(bounds.minimum.z,x.position.z)};bounds.maximum={std::max(bounds.maximum.x,x.position.x),std::max(bounds.maximum.y,x.position.y),std::max(bounds.maximum.z,x.position.z)};}sum+=x.position;++n;}
    std::cout<<m.skeleton.bones[b].name<<" vertices="<<n<<" center=";v(n?sum/float(n):sum);std::cout<<" min=";v(bounds.minimum);std::cout<<" max=";v(bounds.maximum);std::cout<<"\n";
   }
  }
  auto rig=scene::buildScene(cast::Document::load(find("c_usa_mp_isa_smg_viewhands_LOD0")));scene::appendRigModel(cast::Document::load(path),rig,name);
  const auto mag=find("t6_attach_mag_"+(key=="scarh"?std::string("scar"):key)+"_view_LOD0");if(!mag.empty())scene::appendRigModel(cast::Document::load(mag),rig,mag.stem().string());
  if(key=="scarh")key="scar_h";
  for(const auto* action:{"idle","reload","reload_empty"}){
   const auto anim=find("viewmodel_"+key+"_"+action);if(anim.empty())continue;
   const auto idx=rig.animations.size();scene::appendAnimations(cast::Document::load(anim),rig);if(idx==rig.animations.size())continue;
   const auto& clip=rig.animations[idx];std::cout<<"ANIM "<<action<<" frames="<<clip.durationFrames<<"\n";
   if(std::string(action).starts_with("reload"))fits(rig,idx);
   for(float t:{0.f,.25f,.5f,.75f,1.f}){auto pose=rig.samplePose(idx,t*clip.durationFrames);const auto gun=rig.skeleton.boneByCanonicalName.at("j_gun");const auto inv=scene::inverseAffine(pose[gun]);
    for(size_t b=0;b<rig.skeleton.bones.size();++b){const auto& bone=rig.skeleton.bones[b];if(bone.name.find("clip")==std::string::npos)continue;std::cout<<t<<" "<<bone.name<<" delta="<<bone.translationTracksAreDeltas<<" bind=";v(bone.restLocal.position);std::cout<<" sampledGun=";v(scene::transformPoint(inv*pose[b],{}));std::cout<<"\n";}
   }
  }
  if(const auto fit=scene::inferT6MagazineMount(rig)){std::cout<<"CALIBRATED "<<name<<" position=";v(fit->position);std::cout<<" residual="<<fit->residual<<" support="<<fit->supportingWindows<<" incoming="<<fit->incomingSocket<<"\n";}else std::cout<<"UNRESOLVED "<<name<<"\n";
 }
}
