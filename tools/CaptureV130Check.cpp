#include "app/LiveViewportRecorder.h"
#include "scene/CastScene.h"
#include "cast/CastDocument.h"
#include "scene/ColdWarTextures.h"
#include "scene/ColdWarWorld.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<#x<<'\n';std::exit(1);}}while(false)
int main(int argc,char**argv){
 if(argc>1&&std::string(argv[1])=="slide"){
  const std::filesystem::path root="D:/Editing/COD Resource/3D Rip/saluki/exported_files";
  auto actor=scene::buildScene(cast::Document::load(root/"bo2/models/playermodels/isa/c_usa_mp_isa_assault_fb/c_usa_mp_isa_assault_fb_LOD0.cast"),false);
  scene::appendAnimations(cast::Document::load(root/"bocw_sp/animations/pb/run/assault rifle/pb_rifle_run_slide_land_l.cast"),actor);auto adapter=actor.animations[0].coldWarWorldPose;CHECK(adapter);
  const auto dump=[](const scene::CastScene& s,std::vector<scene::Transform> local,const char* label){std::cout<<label<<'\n';for(auto name:{"j_wrist_ri","tag_weapon_right"}){auto i=s.skeleton.boneByCanonicalName.at(name);const auto b=s.skeleton.bones[i].restLocal;const auto a=local[i];std::cout<<name<<" bind p="<<b.position.x<<","<<b.position.y<<","<<b.position.z<<" anim p="<<a.position.x<<","<<a.position.y<<","<<a.position.z<<" bind q="<<b.rotation.x<<","<<b.rotation.y<<","<<b.rotation.z<<","<<b.rotation.w<<" anim q="<<a.rotation.x<<","<<a.rotation.y<<","<<a.rotation.z<<","<<a.rotation.w<<'\n';}};
  dump(actor,actor.sampleLocalPose(0,10),"target");dump(*adapter->source,adapter->source->sampleLocalPose(0,10),"source");return 0;
 }
 if(argc>2&&std::string(argv[1])=="performance"){
  const auto doc=cast::Document::load(argv[2]);CHECK(doc.valid());std::vector<const cast::Property*> arrays;
  const auto visit=[&](auto&&self,const cast::Node& n)->void{for(const auto&p:n.properties)if(p.type=="f"||p.type=="2v"||p.type=="3v"||p.type=="4v")arrays.push_back(&p);for(const auto&c:n.children)self(self,c);};for(const auto&r:doc.roots())visit(visit,r);
  const auto old=[&](const cast::Property&p){const auto data=doc.propertyData(p);std::vector<float> v;const auto n=std::size_t(p.elementCount)*p.componentCount;v.reserve(n);for(std::size_t i=0;i<n;++i){float x{};std::memcpy(&x,data.data()+i*4,4);v.push_back(x);}return v;};
  for(auto*p:arrays){const auto a=old(*p),b=doc.floatValues(*p);CHECK(a.size()==b.size());CHECK(a.empty()||std::memcmp(a.data(),b.data(),a.size()*4)==0);}
  std::size_t consumed=0;const auto bench=[&](bool bulk){const auto t=std::chrono::steady_clock::now();for(int n=0;n<300;++n)for(auto*p:arrays){auto v=bulk?doc.floatValues(*p):old(*p);consumed+=v.size();}return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();};
  const auto baseline=bench(false),bulk=bench(true);std::cout<<"PASS "<<arrays.size()<<" float arrays bit-identical; 300 decode passes old="<<baseline<<"ms bulk="<<bulk<<"ms / "<<consumed<<" values\n";return 0;
 }
 if(argc>1&&std::string(argv[1])=="model"){
  for(int i=2;i<argc;++i){auto doc=cast::Document::load(argv[i]);CHECK(doc.valid());auto s=scene::buildScene(doc,false);std::cout<<"MODEL "<<argv[i]<<'\n';for(const auto& m:s.meshes)std::cout<<m.name<<" | "<<m.materialName<<" | "<<m.albedoPath.string()<<" exists="<<std::filesystem::exists(m.albedoPath)<<'\n';for(std::size_t j=0;j<s.skeleton.bones.size();++j)std::cout<<j<<" "<<s.skeleton.bones[j].name<<" parent="<<s.skeleton.bones[j].parent<<'\n';}return 0;
 }
 CHECK(argc>1);std::filesystem::path folder=argv[1];std::filesystem::create_directories(folder);
 const auto base=folder/"wpn_t9_sniper_standard_scope_view"/"_images"/"mc"/"mtl_wpn_t9_sniper_standard_scope_front";
 std::filesystem::create_directories(base);{std::ofstream f(base/"i_wpn_t9_sniper_standard_scope_front_c.png");f<<"fixture";}
 const auto variant=folder/"wpn_t9_sniper_standard_scope_animesg_view";std::filesystem::create_directories(variant);
 const auto missing=variant/"_images/mc/mtl_wpn_t9_sniper_standard_scope_front_animesg/i_mtl_wpn_t9_sniper_standard_scope_front_animesg_c.png";
 CHECK(scene::coldwar_textures::fallback(variant/"model.cast","mc_mtl_wpn_t9_sniper_standard_scope_front_animesg",missing)==base/"i_wpn_t9_sniper_standard_scope_front_c.png");
 {std::ofstream f(variant/"existing.png");f<<"variant";}CHECK(scene::coldwar_textures::fallback(variant/"model.cast","anything",variant/"existing.png")==variant/"existing.png");
 cadence::capture::CameraFrame c;c.width=321;c.height=181;c.sourceTime=.125;c.position={1,2,3};c.up={0,.5f,.8660254f};
 cadence::capture::CameraCsv csv;CHECK(csv.open(folder/"camera_test.csv"));CHECK(csv.write(0,0,c));c.position.x=NAN;CHECK(!csv.write(1,.1,c));csv.close();CHECK(!csv.open(folder/"camera_test.csv"));c.position.x=1;
 CHECK(cadence::capture::frameAt(.5,60)==30);CHECK(cadence::capture::frameAt(-1,60)==0);
 cadence::capture::LiveViewportRecorder r;CHECK(r.start(folder/"live_test.mp4",321,181,30,18,15,true));
 for(int i=0;i<40;++i){while(!r.due()&&r.active())std::this_thread::sleep_for(std::chrono::milliseconds(1));CHECK(r.active());std::vector<std::uint8_t> px(321*181*4);for(std::size_t j=0;j<px.size();j+=4){px[j]=std::uint8_t(i*5);px[j+1]=100;px[j+2]=200;px[j+3]=255;}CHECK(r.submit(std::move(px),c));}
 r.stop();while(r.active())std::this_thread::sleep_for(std::chrono::milliseconds(5));CHECK(r.error().empty());CHECK(r.frames()>=40);CHECK(std::filesystem::file_size(folder/"live_test.mp4")>1000);
 CHECK(!r.start(folder/"live_test.mp4",321,181,30,18,15,false));
 CHECK(r.start(folder/"resize_test.mp4",321,181,30,18,15,false));c.width=320;CHECK(!r.submit(std::vector<std::uint8_t>(320*181*4),c));while(r.active())std::this_thread::sleep_for(std::chrono::milliseconds(5));CHECK(!r.error().empty());
 std::cout<<"PASS camera CSV finite checks/no-overwrite and actual threaded libx264 encode, resize guard\n";
}
