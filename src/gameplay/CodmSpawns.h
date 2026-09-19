#pragma once
#include "scene/BoundedJson.h"
#include "scene/Math3D.h"
#include <fstream>
#include <filesystem>
#include <algorithm>
namespace gameplay::bot {
struct CodmSpawnSet {std::string mode;std::vector<scene::Vec3> spawns;std::size_t records{},filtered{};};
inline bool readCodmSpawns(std::string_view text,float scale,CodmSpawnSet& output,std::string& error){
 try{
  const auto j=scene::codm::parseJson(text);
  if(j.at("schema")!="codm.spawns/1"||j.at("coordinateSystem")!="RH_Z_UP"||j.at("units")!="inches")throw std::runtime_error("unsupported schema, axes or units");
  if(!std::isfinite(scale)||scale<=0)throw std::runtime_error("invalid map scale");
  const auto& sets=j.at("sets");if(!sets.is_array()||sets.size()>256)throw std::runtime_error("invalid spawn sets");
  std::vector<CodmSpawnSet> candidates;std::size_t total=0;
  for(const auto& set:sets){
   CodmSpawnSet c;c.mode=set.at("mode").get<std::string>();const auto& points=set.at("spawns");
   if(!points.is_array()||(total+=points.size())>100000)throw std::runtime_error("invalid spawn count");
   c.records=points.size();
   for(const auto& entry:points){
    if(!entry.value("enabled",true)||!entry.value("hierarchyActive",true)||!entry.value("available",true)){++c.filtered;continue;}
    const auto& p=entry.at("position");if(!p.is_array()||p.size()!=3)throw std::runtime_error("invalid spawn position");
    scene::Vec3 v;float* values[]={&v.x,&v.y,&v.z};
    for(int i=0;i<3;++i){const double value=p[i].get<double>()*2.54*scale;if(!std::isfinite(value)||std::abs(value)>1e8)throw std::runtime_error("nonfinite/out-of-range position");*values[i]=static_cast<float>(value);}
    if(std::none_of(c.spawns.begin(),c.spawns.end(),[&](scene::Vec3 a){return scene::length(a-v)<.1f;}))c.spawns.push_back(v);
   }
   candidates.push_back(std::move(c));
  }
  auto rank=[](const std::string& m){return m=="Main_TDM"?0:m=="Main_FFA"?1:m=="Main_KC"?2:3;};
  std::stable_sort(candidates.begin(),candidates.end(),[&](const auto& a,const auto& b){const int x=rank(a.mode),y=rank(b.mode);return x!=y?x<y:a.mode<b.mode;});
  CodmSpawnSet chosen;for(auto& c:candidates)if(!c.spawns.empty()){chosen=std::move(c);break;}
  output=std::move(chosen);error.clear();return true;
 }catch(const std::exception& e){error="CODM spawns.json: "+std::string(e.what());return false;}
}
inline bool mergeCodmSpawns(std::string_view existing,float scale,const std::vector<scene::Vec3>& points,std::string& result,std::string& error){
 try{
  using scene::codm::Json;Json root;CodmSpawnSet selected;
  if(!existing.empty()){if(!readCodmSpawns(existing,scale,selected,error))return false;root=scene::codm::parseJson(existing);}
  else{root={{"schema","codm.spawns/1"},{"generator","Cadence"},{"coordinateSystem","RH_Z_UP"},{"units","inches"},{"status","found"},{"complete",true},{"sets",Json::array()}};}
  if(!std::isfinite(scale)||scale<=0)throw std::runtime_error("invalid map scale");
  const std::string mode=selected.mode.empty()?"Main_TDM":selected.mode;auto& sets=root["sets"];Json* target=nullptr;
  for(auto& set:sets)if(set.at("mode")==mode){target=&set;break;}
  if(!target){sets.push_back({{"mode",mode},{"scene","Cadence"},{"spawns",Json::array()}});target=&sets.back();}
  auto& spawns=(*target)["spawns"];std::unordered_set<std::string> ids;for(const auto& set:sets)for(const auto& p:set["spawns"])if(p.contains("id"))ids.insert(p["id"].get<std::string>());
  for(const auto p:points){
   if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||std::max({std::abs(p.x),std::abs(p.y),std::abs(p.z)})>1e8)throw std::runtime_error("invalid saved point");
   if(std::any_of(selected.spawns.begin(),selected.spawns.end(),[&](scene::Vec3 q){return scene::length(q-p)<.1f;}))continue;
   std::size_t serial=spawns.size()+1;std::string id;do{id="cadence:"+mode+":"+std::to_string(serial++);}while(ids.contains(id));ids.insert(id);
   const double units=2.54*scale,x=p.x/units,y=p.y/units,z=p.z/units;
   spawns.push_back({{"id",id},{"name","CadenceSpawn"},{"position",{x,y,z}},{"forward",{1,0,0}},{"up",{0,0,1}},
    {"glbPositionMetres",{x*.0254,z*.0254,-y*.0254}},{"glbForward",{1,0,0}},{"glbUp",{0,1,0}},
    {"enabled",true},{"hierarchyActive",true},{"available",true},{"initialSpawn",false},{"camp",0},{"group",0},{"aiOnly",false},{"cadenceAuthored",true}});
   selected.spawns.push_back(p);
  }
  std::size_t count=0;for(const auto& set:sets)count+=set["spawns"].size();root["spawnCount"]=count;
  if(count)root["status"]="found";result=root.dump(2)+"\n";
  CodmSpawnSet validated;if(!readCodmSpawns(result,scale,validated,error))return false;
  error.clear();return true;
 }catch(const std::exception& e){error="Save CODM spawns: "+std::string(e.what());return false;}
}
inline bool loadCodmSpawns(const std::filesystem::path& path,float scale,CodmSpawnSet& output,std::string& error){
 std::error_code ec;const auto size=std::filesystem::file_size(path,ec);if(ec||size>16*1024*1024){error="CODM spawns.json missing, unreadable or larger than 16 MiB";return false;}
 std::ifstream in(path,std::ios::binary);std::string text(static_cast<std::size_t>(size),'\0');if(!in.read(text.data(),static_cast<std::streamsize>(size))){error="Could not read CODM spawns.json";return false;}
 return readCodmSpawns(text,scale,output,error);
}
}
