#include "scene/ImportedBody.h"
#include "assets/ImportedGamePolicy.h"
#include <iostream>
int main(int argc,char** argv){if(argc!=2)return 2;scene::CastScene base,assembled;std::size_t count{},compatible{},vertices{};
 for(const auto& entry:std::filesystem::directory_iterator(argv[1])){if(entry.path().extension()!=".cast"||entry.path().stem().string().starts_with("viewmodel_")||entry.path().stem().string().starts_with("hand_")||entry.path().stem().string().starts_with("w_"))continue;
  auto s=scene::buildScene(cast::Document::load(entry.path()),false);if(!scene::imported::bodyLayout(s.skeleton))continue;if(base.meshes.empty())base=s;
  float error{};int missing{};for(const auto& b:s.skeleton.bones){const auto it=base.skeleton.boneByName.find(b.name);if(it==base.skeleton.boneByName.end()){++missing;continue;}for(int k=0;k<16;++k)error=std::max(error,std::abs(b.restGlobal.v[k]-base.skeleton.bones[it->second].restGlobal.v[k]));}
  const bool match=scene::imported::sharedBodyAssemblyCompatible(base,s);++count;compatible+=match;std::cout<<entry.path().filename().string()<<" bones="<<s.skeleton.bones.size()<<" missing="<<missing<<" matrix_error="<<error<<" compatible="<<match<<'\n';
  if(!match)continue;
  if(assembled.skeleton.bones.empty()){assembled=base;assembled.meshes.clear();assembled.rigParts.clear();assembled.attachments.clear();}
  const auto first=assembled.meshes.size();scene::appendRigModel(cast::Document::load(entry.path()),assembled,entry.path().stem().string());
  if(assembled.meshes.size()-first!=s.meshes.size())return 3;
  for(std::size_t m=0;m<s.meshes.size();++m){const auto& source=s.meshes[m];const auto& merged=assembled.meshes[first+m];
   if(source.vertices.size()!=merged.vertices.size()||source.indices!=merged.indices||source.skinned!=merged.skinned)return 4;
   for(std::size_t v=0;v<source.vertices.size();++v){const auto& a=source.vertices[v];const auto& b=merged.vertices[v];++vertices;
    if(scene::length(a.position-b.position)>.0001f||scene::length(a.normal-b.normal)>.0001f||a.uv.x!=b.uv.x||a.uv.y!=b.uv.y||a.weights!=b.weights)return 5;
    if(!source.skinned)continue;
    for(std::size_t k=0;k<4;++k)if(a.weights[k]>0){const auto& x=s.skeleton.bones.at(a.bones[k]);const auto& y=assembled.skeleton.bones.at(b.bones[k]);if(x.name!=y.name)return 6;
     for(std::size_t n=0;n<16;++n)if(std::abs(x.restGlobal.v[n]-y.restGlobal.v[n])>.0001f||std::abs(x.inverseBind.v[n]-y.inverseBind.v[n])>.0001f)return 7;
    }
   }
  }
 }std::cout<<"models="<<count<<" compatible="<<compatible<<" assembled_vertices_verified="<<vertices<<'\n';}
