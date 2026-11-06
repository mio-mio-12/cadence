#include "assets/AssetCatalog.h"
#include "assets/Source2Skins.h"
#include "scene/CastScene.h"
#include "scene/ImportedBodyRetarget.h"
#include <iostream>
int main(int argc,char** argv){
 if(argc==3&&std::string(argv[1])=="--deaths"){
  for(const auto& e:std::filesystem::directory_iterator(argv[2]))if(e.path().filename().string().find("death")!=std::string::npos){auto d=cast::Document::load(e.path());auto s=scene::buildScene(d);for(const auto& r:d.roots())for(const auto& p:r.properties)if(p.stringValue)std::cout<<p.name<<"="<<*p.stringValue<<' ';for(const auto& a:s.animations)std::cout<<e.path().filename().string()<<" name="<<a.name<<" frames="<<a.durationFrames<<'\n';}return 0;
 }
 if(argc==4&&std::string(argv[1])=="--body-layers"){
  const auto model=scene::buildScene(cast::Document::load(argv[2]),false);
  const auto layout=scene::imported::bodyLayout(model.skeleton);
  auto library=model;
  for(const auto& entry:std::filesystem::directory_iterator(argv[3])){
   const auto name=entry.path().filename().string();
   if(!name.starts_with("pb_")||name.find("_idle_")==std::string::npos)continue;
   scene::CastScene s;s.skeleton=model.skeleton;scene::appendAnimations(cast::Document::load(entry.path()),s);
   for(auto& a:s.animations){a.sourceGame="cso2";scene::imported::classifySourceBodyAnimation(a);size_t legs{};
    for(const auto& t:a.tracks)for(int side=0;side<2;++side)if(int(t.boneIndex)==layout.thigh[side]||int(t.boneIndex)==layout.knee[side]||int(t.boneIndex)==layout.foot[side])++legs;
    scene::imported::classifyNativeGaitLayer(s.skeleton,a);
    std::cout<<name<<" tracks="<<a.tracks.size()<<" legs="<<legs<<" domain="<<int(a.domain)<<'\n';
    library.animations.push_back(a);
   }
  }
  for(const auto weapon:{scene::WeaponClass::Knife,scene::WeaponClass::Pistol,scene::WeaponClass::Rifle}){
   scene::AnimationQuery q;q.domain=scene::AnimationDomain::PlayerBody;q.motion=scene::MotionRole::Idle;q.weapon=weapon;q.stance=scene::Stance::Stand;q.preferredGame="cso2";
   const auto base=scene::findBestAnimation(library,q);if(!base){std::cerr<<"Missing body idle\n";return 10;}
   size_t legs{};for(const auto& t:library.animations[*base].tracks)for(int side=0;side<2;++side)if(int(t.boneIndex)==layout.thigh[side]||int(t.boneIndex)==layout.knee[side]||int(t.boneIndex)==layout.foot[side])++legs;
   std::cout<<"SELECTED weapon="<<int(weapon)<<" legs="<<legs<<" clip="<<library.animations[*base].sourceName<<'\n';if(!legs)return 11;
  }return 0;
 }
 if(argc==4&&std::string(argv[1])=="--bind"){
  auto model=scene::buildScene(cast::Document::load(argv[2])),driver=scene::buildScene(cast::Document::load(argv[3]));
  std::cout<<"model bones="<<model.skeleton.bones.size()<<" driver="<<driver.skeleton.bones.size()<<'\n';
  for(const auto& m:model.meshes){scene::Vec3 lo{1e9f,1e9f,1e9f},hi{-1e9f,-1e9f,-1e9f};for(const auto& v:m.vertices){lo={std::min(lo.x,v.position.x),std::min(lo.y,v.position.y),std::min(lo.z,v.position.z)};hi={std::max(hi.x,v.position.x),std::max(hi.y,v.position.y),std::max(hi.z,v.position.z)};}std::cout<<"MESH "<<m.name<<" skin="<<m.skinned<<" bounds="<<lo.x<<','<<lo.y<<','<<lo.z<<":"<<hi.x<<','<<hi.y<<','<<hi.z<<'\n';}
  if(!driver.animations.empty()){const auto p=driver.samplePose(0,0);for(size_t i=0;i<p.size();++i)if(driver.skeleton.bones[i].name=="wpn"||driver.skeleton.bones[i].name=="weapon"||driver.skeleton.bones[i].name=="camera")std::cout<<"POSE "<<driver.skeleton.bones[i].name<<' '<<p[i].v[12]<<','<<p[i].v[13]<<','<<p[i].v[14]<<'\n';}
  for(const auto& b:model.skeleton.bones){const auto found=driver.skeleton.boneByCanonicalName.find(scene::canonicalName(b.name));
   if(found==driver.skeleton.boneByCanonicalName.end()){std::cout<<"UNMATCHED "<<b.name<<'\n';continue;}
   const auto& d=driver.skeleton.bones[found->second];const auto correction=d.restGlobal*b.inverseBind;
   float delta{};for(int i=0;i<16;++i)delta=std::max(delta,std::abs(correction.v[i]-(i%5==0?1.f:0.f)));
   std::cout<<"BIND "<<b.name<<" parent="<<b.parent<<" driverParent="<<d.parent<<" correction="<<delta<<" model="<<b.restGlobal.v[12]<<','<<b.restGlobal.v[13]<<','<<b.restGlobal.v[14]<<" driver="<<d.restGlobal.v[12]<<','<<d.restGlobal.v[13]<<','<<d.restGlobal.v[14]<<'\n';
  }
  auto merged=driver;merged.viewHandsDriverGame="cs2";merged.meshes.clear();
  scene::appendRigModel(cast::Document::load(argv[2]),merged,"weapon");
  if(const auto w=merged.skeleton.boneByName.find("weapon"),p=merged.skeleton.boneByName.find("wpn");w!=merged.skeleton.boneByName.end()&&p!=merged.skeleton.boneByName.end()){
   if(merged.skeleton.bones[w->second].parent!=static_cast<int>(p->second)){std::cerr<<"Weapon disconnected from animated socket\n";return 12;}
  }
  float maximum{};size_t checked{};
  if(driver.animations.empty())return 8;
  for(float t:{0.f,.5f,1.f}){
   const auto pose=merged.samplePose(0,driver.animations[0].durationFrames*t);
   for(size_t mi=0;mi<model.meshes.size();++mi)for(size_t vi=0;vi<model.meshes[mi].vertices.size();++vi){
    const auto& v=model.meshes[mi].vertices[vi];const auto& got=merged.meshes[mi].vertices[vi];scene::Vec3 expected{},actual{};bool all=true;
    for(size_t k=0;k<4;++k)if(v.weights[k]>0){const auto& source=model.skeleton.bones[v.bones[k]];const auto found=driver.skeleton.boneByCanonicalName.find(scene::canonicalName(source.name));if(found==driver.skeleton.boneByCanonicalName.end()){all=false;break;}expected+=scene::transformPoint(pose[found->second]*source.inverseBind,v.position)*v.weights[k];actual+=scene::transformPoint(pose[got.bones[k]]*merged.skeleton.bones[got.bones[k]].inverseBind,got.position)*got.weights[k];}
    if(all){maximum=std::max(maximum,scene::length(expected-actual));++checked;}
   }
  }
  std::cout<<"WEAPON_BIND checked="<<checked<<" max_error="<<maximum<<'\n';return checked&&maximum<.01f?0:9;
 }
 if(argc<2)return 2;assets::Catalog catalog;std::string error;
 if(!assets::appendScan(argv[1],"cs2",catalog,error)){std::cerr<<error;return 3;}
 for(const auto& a:catalog.entries){auto doc=cast::Document::load(a.path);if(!doc.valid())return 4;
  auto s=scene::buildScene(doc,false);
  std::cout<<a.name<<" role="<<assets::roleName(a.role)<<" skin="<<assets::source2::skin(a)<<" meshes="<<s.meshes.size()<<" bones="<<s.skeleton.bones.size()<<" body="<<scene::imported::completeBodyLayout(scene::imported::bodyOrCodLayout(s.skeleton))<<'\n';
  if(argc>2&&(a.name=="ctm_sas"||a.role==assets::Role::ViewHands))for(const auto& b:s.skeleton.bones)std::cout<<" BONE "<<b.name<<" parent="<<b.parent<<" pos="<<b.restGlobal.v[12]<<','<<b.restGlobal.v[13]<<','<<b.restGlobal.v[14]<<'\n';
 }
}
