#include "scene/C2mxMap.h"
#include "scene/C2MMap.h"
#include <iostream>
#include <cassert>
#include <fstream>
#include <set>
using scene::codm::Json;
static Json meta(){return {{"schema","codm.c2mx/1"},{"collisionPolicy","authored_only"},{"collisionComplete",true}};}
static Json nav(){return {{"schema","codm.navigation/1"},{"status","settings_only"},{"runtimeReady",false},{"assets",Json::array()}};}
static Json box(){return {{"id","invisible:1"},{"name","InvisibleWall"},{"kind","BoxCollider"},{"layer",14},{"enabled",true},{"trigger",false},{"center",{0,0,0}},{"size",{20,30,40}},{"matrix",{1,0,0,0,0,1,0,0,0,0,1,0,10,20,30,1}}};}
static Json coll(Json items){return {{"schema","codm.collision/1"},{"units","inches"},{"coordinateSystem","RH_Z_UP"},{"colliders",items}};}
static bool testCollision(scene::glb::Map& map,const Json& items,const scene::c2m::LoadOptions& options={}){std::string error;const auto okay=scene::c2mx::applyCollision(coll(items),meta(),nav(),map,error,options);if(!okay)std::cout<<"expected/observed validation: "<<error<<'\n';return okay;}
static void put(std::vector<std::uint8_t>& b,std::uint64_t value,unsigned count){for(unsigned i=0;i<count;++i)b.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
static std::vector<std::uint8_t> envelope(std::vector<std::uint8_t> bytes=std::vector<std::uint8_t>(128),Json metadata=meta()){
 std::vector<std::array<std::uint64_t,2>> spans;
 for(const auto& j:{metadata,coll(Json::array()),nav()}){while(bytes.size()%8)bytes.push_back(0);const auto s=j.dump();spans.push_back({bytes.size(),s.size()});bytes.insert(bytes.end(),s.begin(),s.end());}
 while(bytes.size()%8)bytes.push_back(0);const auto directory=bytes.size();int i=0;
 for(const auto* tag:{"META","COLL","NAVM"}){bytes.insert(bytes.end(),tag,tag+4);put(bytes,1,4);put(bytes,spans[i][0],8);put(bytes,spans[i++][1],8);}
 for(char c:std::string("C2MX"))bytes.push_back(c);put(bytes,1,4);put(bytes,directory,8);put(bytes,72,8);put(bytes,3,4);put(bytes,0,4);return bytes;
}
static std::vector<std::uint8_t> legacyFloor(){
 std::vector<std::uint8_t> bytes{'C','2','M',3,5};const auto str=[&](std::string s){bytes.push_back(1);bytes.insert(bytes.end(),s.begin(),s.end());bytes.push_back(0);};const auto f=[&](float value){std::uint32_t raw;std::memcpy(&raw,&value,4);put(bytes,raw,4);};
 str("test_floor");str("");const auto table=bytes.size();bytes.resize(table+80);const auto object=bytes.size();str("mapGeometry");put(bytes,4,4);put(bytes,1,4);put(bytes,2,4);put(bytes,0,4);f(0);
 for(auto v:{scene::Vec3{0,0,0},scene::Vec3{40,0,0},scene::Vec3{40,40,0},scene::Vec3{0,40,0}}){f(v.x);f(v.y);f(v.z);}for(int i=0;i<4;++i){f(0);f(0);f(1);}for(int i=0;i<4;++i){put(bytes,1,4);f(0);f(0);}for(int i=0;i<16;++i)bytes.push_back(255);
 str("floor");put(bytes,0,8);put(bytes,1,1);put(bytes,1,1);put(bytes,0,2);put(bytes,2,4);for(auto i:{0,1,2,0,2,3})put(bytes,i,4);
 const auto materials=bytes.size();str("plain_floor");str("opaque");str("");for(int i=0;i<4;++i)put(bytes,0,1);const auto ents=bytes.size();str("");
 std::vector<std::uint8_t> header;put(header,1,4);put(header,object,8);put(header,0,4);put(header,0,4);put(header,materials,8);put(header,0,4);put(header,materials,8);put(header,1,4);put(header,materials,8);put(header,0,4);put(header,ents,8);put(header,ents,8);assert(header.size()==72);std::copy(header.begin(),header.end(),bytes.begin()+table);return bytes;
}
int main(int argc,char** argv){
 {
    scene::glb::Map fallback;
    auto floor=box();floor["size"]={400,400,10};floor["matrix"]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    auto floors=Json::array({floor});
    for(int y=0;y<3;++y)for(int x=0;x<3;++x){floor["id"]="floor_"+std::to_string(y*3+x);floor["matrix"][12]=10000+x*400;floor["matrix"][13]=20000+y*400;floors.push_back(floor);}
    assert(testCollision(fallback,floors));assert(fallback.hasDefaultSpawnPoint&&fallback.defaultSpawnPoint.x>20000&&fallback.defaultSpawnPoint.y>40000);
    const auto p=fallback.defaultSpawnPoint;
    fallback.scene.bounds.minimum={-1e7f,-1e7f,-1e7f};fallback.scene.bounds.maximum={1e7f,1e7f,1e7f};fallback.scene.bounds.valid=true;
    fallback.authoredCollision.physicsReady=false;
    assert(!scene::c2mx::chooseAuthoredSpawn(fallback)&&!fallback.hasDefaultSpawnPoint&&fallback.collisionPreviewPoint);
    assert(scene::length(*fallback.collisionPreviewPoint-p)<.01f); // Preview does not approve collision or use huge render bounds.
    assert(testCollision(fallback,Json::array()));assert(!fallback.hasDefaultSpawnPoint&&!fallback.collisionPreviewPoint);
 }
 scene::glb::Map map;std::string error;
 assert(testCollision(map,Json::array({box()})));assert(map.collision.size()==12&&map.authoredCollision.physicsReady&&map.authoredCollision.colliderCount==1);assert(!map.hasDefaultSpawnPoint); // Too narrow for a standing capsule.
 const float top=50*2.54f;assert(std::abs(map.groundHeight(25.4f,50.8f,1000,-999)-top)<.01f);assert(map.authoredColliders[0].layer==14);
 // Mirrored, rotated and nonuniform local box: compare actual transformed corners.
 auto b=box();b["matrix"]={0,-2,0,0,-3,0,0,0,0,0,4,0,100,200,300,1};assert(testCollision(map,Json::array({b})));
 assert(std::abs(map.authoredColliders[0].bounds.maximum.z-380*2.54f)<.02f);assert(std::abs(map.groundHeight(254,508,2000,-999)-380*2.54f)<.02f);
 b=box();b["trigger"]=true;assert(testCollision(map,Json::array({b})));assert(map.collision.empty()&&map.authoredTriggerTriangles.size()==12);assert(scene::c2mx::triggerIdsAtPoint(map,{25.4f,50.8f,76.2f}).size()==1);assert(scene::c2mx::triggerIdsAtPoint(map,{999,999,999}).empty());assert(map.groundHeight(25.4f,50.8f,1000,-999)==-999);
 Json mesh=box();mesh["kind"]="MeshCollider";mesh["center"]={0,0,0};mesh["matrix"]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};mesh["vertices"]={{10,20,30},{30,20,30},{10,40,30}};mesh["triangles"]={{0,1,2}};mesh["convex"]=false;map.scaleMultiplier=2;
 assert(testCollision(map,Json::array({mesh})));assert(map.collision.size()==1&&std::abs(map.collision[0].a.x-50.8f)<.01f);
 const auto before=map.collision[0].a;mesh["triangles"]={{0,1,9}};assert(!testCollision(map,Json::array({mesh})));assert(scene::length(before-map.collision[0].a)<.001f);
 mesh["triangles"]={{0,1,2}};mesh["convex"]=true;assert(testCollision(map,Json::array({mesh})));assert(!map.authoredCollision.physicsReady&&map.collision.empty());
 b=box();b["matrix"][4]=.5;assert(testCollision(map,Json::array({b})));assert(map.authoredCollision.physicsReady&&map.collision.size()==12);
 assert(std::abs(map.authoredColliders[0].bounds.maximum.x-27.5f*5.08f)<.02f);
 b["matrix"][0]=-1;assert(testCollision(map,Json::array({b})));assert(map.authoredCollision.physicsReady);
 assert(std::abs(map.groundHeight(50.8f,101.6f,1000,-999)-50*5.08f)<.02f);
 b["matrix"][0]=0;b["matrix"][1]=1;b["matrix"][4]=0;assert(!testCollision(map,Json::array({b}))); // Collinear basis rejected.
 b=box();b["layer"]=64;assert(!testCollision(map,Json::array({b})));b=box();b["size"]={-1,2,3};assert(!testCollision(map,Json::array({b})));b=box();b["matrix"]={1,2};assert(!testCollision(map,Json::array({b})));
 b=box();b["kind"]="SphereCollider";b["radius"]=5;b["scalePolicy"]="unity_max_axis";b["matrix"]={2,0,0,0,0,3,0,0,0,0,4,0,0,0,0,1};map.scaleMultiplier=1;
 assert(testCollision(map,Json::array({b})));assert(map.authoredCollision.approximate&&!map.authoredCollision.physicsReady);scene::c2m::LoadOptions accepted;accepted.allowApproximateCollision=true;
 assert(testCollision(map,Json::array({b}),accepted));assert(map.authoredCollision.physicsReady);const auto bounds=map.authoredColliders[0].bounds;assert(std::abs(bounds.maximum.x-50.8f)<.01f&&std::abs(bounds.maximum.z-50.8f)<.01f);
 b["kind"]="CapsuleCollider";b["axis"]=2;b["height"]=30;b["scalePolicy"]="unity_axis_height_max_perpendicular_radius";assert(testCollision(map,Json::array({b}),accepted));assert(std::abs(map.authoredColliders[0].bounds.maximum.z-60*2.54f)<.02f);assert(std::abs(map.authoredColliders[0].bounds.maximum.x-15*2.54f)<.02f);
 assert(testCollision(map,Json::array()));assert(map.collision.empty()&&map.authoredCollision.physicsReady&&!map.hasDefaultSpawnPoint);assert(!map.authoredCollision.nativeNavigationReady&&map.authoredCollision.navigationStatus=="settings_only");
 auto bytes=envelope();assert(scene::c2mx::readExtension(bytes).present);
 const auto rejected=[&](auto corrupt){try{scene::c2mx::readExtension(corrupt);return false;}catch(...){return true;}};
 auto bad=bytes;bad.pop_back();assert(rejected(bad));bad=bytes;bad[bad.size()-28]=2;assert(rejected(bad));
 const auto directory=bytes.size()-32-72;bad=bytes;for(int i=0;i<8;++i)bad[directory+24+8+i]=bad[directory+8+i];assert(rejected(bad));bad=bytes;bad[directory+4]=2;assert(rejected(bad));bad=bytes;std::copy_n(bad.begin()+directory,4,bad.begin()+directory+24);assert(rejected(bad));
 try{scene::codm::parseJson("{\"x\":1,\"x\":2}");assert(false);}catch(...){}try{scene::codm::parseJson("{\"x\":1e999}");assert(false);}catch(...){}
 {
  const auto folder=std::filesystem::temp_directory_path()/("cadence_c2mx_test_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directory(folder);const auto legacy=legacyFloor();
  const auto write=[&](const auto& p,const auto& b){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());};write(folder/"old.c2m",legacy);
  assert(scene::c2m::load(folder/"old.c2m",map,error));assert(map.collision.size()==2&&!map.authoredCollision.present&&map.hasDefaultSpawnPoint);
  auto metadata=meta();metadata["visualMaterials"]=Json::array({{{"c2mMaterialName","plain_floor"},{"alpha","OPAQUE"}}});write(folder/"empty.c2m",envelope(legacy,metadata));
  assert(scene::c2m::load(folder/"empty.c2m",map,error));assert(map.collision.empty()&&map.authoredCollision.present&&!map.hasDefaultSpawnPoint&&map.scene.meshes[0].indices.size()==6);
  metadata["visualMaterials"][0]["textures"]={{"color","../outside.png"}};write(folder/"bad.c2m",envelope(legacy,metadata));const auto oldTriangles=map.scene.meshes[0].indices.size();assert(!scene::c2m::load(folder/"bad.c2m",map,error));assert(map.scene.meshes[0].indices.size()==oldTriangles&&map.collision.empty());
  auto safeBox=box();safeBox["size"]={100,100,40};write(folder/"collision.json",coll(Json::array({safeBox})).dump());write(folder/"report.json",meta().dump());write(folder/"navigation.json",nav().dump());
  assert(scene::c2mx::loadCollisionSidecar(folder/"collision.json",map,error));assert(map.collision.size()==12&&map.hasDefaultSpawnPoint&&map.scene.meshes[0].indices.size()==oldTriangles);
  write(folder/"collision.json",coll(Json::array()).dump());assert(scene::c2mx::loadCollisionSidecar(folder/"collision.json",map,error));assert(map.collision.empty()&&!map.hasDefaultSpawnPoint);
  write(folder/"collision.json",std::string("{bad JSON"));assert(!scene::c2mx::loadCollisionSidecar(folder/"collision.json",map,error));assert(map.collision.empty()&&map.authoredCollision.present);
  std::filesystem::remove_all(folder);
 }
 if(argc>1){if(!scene::c2m::load(argv[1],map,error)){std::cerr<<error<<std::endl;return 2;}std::size_t triangles=0;for(const auto& m:map.scene.meshes)triangles+=m.indices.size()/3;std::cout<<"FIXTURE triangles="<<triangles<<" colliders="<<map.authoredCollision.colliderCount<<" collisionTriangles="<<map.collision.size()<<" status="<<map.authoredCollision.status<<'\n';assert(triangles==521133&&map.authoredCollision.colliderCount==575&&map.authoredCollision.physicsReady);
  
  std::size_t wetTriangles=0;std::set<std::string> wetSources;
  for(const auto& mesh:map.scene.meshes)if(mesh.sourceMaterialMetadata){const auto material=scene::codm::parseJson(*mesh.sourceMaterialMetadata);if(material.contains("vertexBlend")&&material["vertexBlend"].contains("keywords")){
   const auto& keywords=material["vertexBlend"]["keywords"];
   if(std::find(keywords.begin(),keywords.end(),Json("_use_g_control_wet_ON"))!=keywords.end()){wetTriangles+=mesh.indices.size()/3;wetSources.insert(material["vertexBlendBake"]["sourceAttributes"].get<std::string>());assert(mesh.vertexBlendBaked&&!mesh.useVertexColor&&mesh.gltfPbr);for(const auto& vertex:mesh.vertices)assert(vertex.color.x==1&&vertex.color.y==1&&vertex.color.z==1&&vertex.color.w==1);}
  }}
  std::cout<<"WET GROUND sourceSurfaces="<<wetSources.size()<<" triangles="<<wetTriangles<<std::endl;assert(wetSources.size()==6&&wetTriangles==19173);
  auto glbPath=std::filesystem::path(argv[1]);glbPath.replace_extension(".glb");
  if(std::filesystem::is_regular_file(glbPath)){
   scene::glb::Map other;if(!scene::glb::load(glbPath,other,error,1.0f,false)){std::cerr<<error<<std::endl;return 3;}
   assert(other.collision.empty()&&other.gridCollision.empty()&&!other.hasDefaultSpawnPoint);
   if(!scene::c2mx::loadCollisionSidecar(glbPath.parent_path()/"collision.json",other,error)){std::cerr<<error<<std::endl;return 4;}
   std::size_t glbTriangles=0;for(const auto& m:other.scene.meshes)glbTriangles+=m.indices.size()/3;
   const auto minError=scene::length(other.scene.bounds.minimum-map.scene.bounds.minimum),maxError=scene::length(other.scene.bounds.maximum-map.scene.bounds.maximum);
   std::cout<<"GLB SIDECAR triangles="<<glbTriangles<<" colliders="<<other.authoredCollision.colliderCount<<" solidTriangles="<<other.collision.size()<<" boundsErrorCm="<<minError<<","<<maxError<<" spawnErrorCm="<<scene::length(other.defaultSpawnPoint-map.defaultSpawnPoint)<<std::endl;
   assert(other.authoredCollision.physicsReady&&other.authoredCollision.colliderCount==575&&other.collision.size()==map.collision.size()&&other.hasDefaultSpawnPoint);
   
   const auto usedBounds=[](const scene::CastScene& scene){scene::Bounds b;for(const auto& m:scene.meshes)for(const auto index:m.indices){const auto p=m.vertices[index].position;if(!b.valid){b.minimum=b.maximum=p;b.valid=true;}else{b.minimum={std::min(b.minimum.x,p.x),std::min(b.minimum.y,p.y),std::min(b.minimum.z,p.z)};b.maximum={std::max(b.maximum.x,p.x),std::max(b.maximum.y,p.y),std::max(b.maximum.z,p.z)};}}return b;};
   const auto c2mBounds=usedBounds(map.scene),glbBounds=usedBounds(other.scene);
   const float usedMin=scene::length(c2mBounds.minimum-glbBounds.minimum),usedMax=scene::length(c2mBounds.maximum-glbBounds.maximum);
   std::cout<<"REFERENCED BOUNDS errorCm="<<usedMin<<","<<usedMax<<" C2M min="<<c2mBounds.minimum.x<<","<<c2mBounds.minimum.y<<","<<c2mBounds.minimum.z<<" GLB min="<<glbBounds.minimum.x<<","<<glbBounds.minimum.y<<","<<glbBounds.minimum.z<<std::endl;
   assert(glbTriangles==triangles&&usedMin<std::max(.1f,scene::length(c2mBounds.minimum)*2e-7f)&&usedMax<std::max(.1f,scene::length(c2mBounds.maximum)*2e-7f)&&scene::length(other.defaultSpawnPoint-map.defaultSpawnPoint)<.1f);

   other.scaleMultiplier=2;assert(scene::c2mx::loadCollisionSidecar(glbPath.parent_path()/"collision.json",other,error));assert(scene::length(other.collision.front().a-map.collision.front().a*2)<.01f);
  }
}
 std::cout<<"C2MX envelope, authored replacement, primitive transforms, triggers, malformed data and navigation tests passed\n";
}
