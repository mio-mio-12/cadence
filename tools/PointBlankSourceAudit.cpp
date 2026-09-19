#include "scene/PointBlankNative.h"
#include "third_party/nlohmann_json.hpp"
#include <fstream>
#include <iostream>
using Json=nlohmann::json;
std::vector<scene::Mat4> sample(const Json& model,const Json& clip,int frame){
 std::vector<std::vector<float>> local;std::map<std::string,size_t> ids;
 for(auto& b:model["bones"]){ids[b["name"].get<std::string>()]=local.size();local.push_back(b["local"].get<std::vector<float>>());}
 if(clip.is_object())for(auto&t:clip["tracks"]){auto it=ids.find(t["name"].get<std::string>());if(it==ids.end()||t["samples"].empty())continue;auto v=t["samples"][std::min(frame,int(t["samples"].size()-1))].get<std::vector<float>>();int flags=t["flags"];for(int k=0;k<10;++k)if(flags&(k<3?1:k<7?2:4))local[it->second][k]=v[k];}
 std::vector<scene::Mat4> out;for(size_t i=0;i<local.size();++i){auto&v=local[i];auto m=scene::trs({v[0],v[1],v[2]},{v[3],v[4],v[5],v[6]},{v[7],v[8],v[9]});int parent=model["bones"][i]["parent"];out.push_back(parent>=0?out[parent]*m:m);}return out;
}
int main(int argc,char**argv){if(argc<5)return 2;Json j;std::ifstream in(argv[1]);in>>j;scene::CastScene s;std::string error;if(!scene::pointblank::assemble(cast::Document::load(argv[2]),cast::Document::load(argv[3]),s,error)){std::cerr<<error;return 3;}scene::appendAnimations(cast::Document::load(argv[4]),s);if(s.animations.empty())return 4;const auto action=std::filesystem::path(argv[4]).stem().string().substr(s.pointBlankWeaponStem.size()+1);Json clip;for(auto&c:j["clips"])if(c["name"]==action)clip=c;if(clip.is_null())return 5;
 float worst=0;std::string worstBone;
 for(int f=0;f<=s.animations[0].durationFrames;++f){auto arms=sample(j["arms"],clip["arms"],f),gun=sample(j["weapon"],clip["weapon"],f),actual=s.samplePose(0,float(f));const int socket=j["arms"]["sockets"][clip["socket"].get<std::string>()];
  for(int kind=0;kind<2;++kind){auto&model=j[kind?"weapon":"arms"];auto&pose=kind?gun:arms;for(size_t b=0;b<pose.size();++b){std::string name=(kind?"pb2cast_weapon__":"")+model["bones"][b]["name"].get<std::string>();auto it=s.skeleton.boneByName.find(name);if(it==s.skeleton.boneByName.end())continue;auto expected=kind?arms[socket]*pose[b]:pose[b];for(int k:{12,13,14})expected.v[k]*=100;float err=0;for(int k=0;k<16;++k)err=std::max(err,std::abs(expected.v[k]-actual[it->second].v[k]));if(err>worst){worst=err;worstBone=name;}if(f==0&&err>.01f)std::cout<<"frame0 "<<name<<" error="<<err<<"\n";}}
 }std::cout<<action<<" max component error="<<worst<<" bone="<<worstBone<<"\n";return worst>.01f?1:0;
}
