#include "scene/PointBlankNative.h"
#include "app/PointBlankAnimationPolicy.h"
#include <iostream>
#include <map>
int main(int argc,char**argv){
 if(argc<4)return 2;scene::CastScene s;std::string error;if(!scene::pointblank::assemble(cast::Document::load(argv[1]),cast::Document::load(argv[2]),s,error)){std::cerr<<error;return 1;}
 auto doc=cast::Document::load(argv[3]);scene::appendAnimations(doc,s);if(s.animations.empty())return 3;
 for(auto&r:doc.roots())for(auto&a:r.children)for(auto&c:a.children)if(auto*n=c.findProperty("nn"))if(n->stringValue&&!s.skeleton.boneByName.contains(*n->stringValue))std::cout<<"UNMAPPED "<<*n->stringValue<<"\n";
 std::vector<scene::Transform> rest;for(auto&b:s.skeleton.bones)rest.push_back(b.restLocal);auto global=s.globalPose(rest);
 for(size_t i=0;i<rest.size();++i){float d=0;for(int k=0;k<16;++k)d=std::max(d,std::abs(global[i].v[k]-s.skeleton.bones[i].restGlobal.v[k]));if(d>.01f)std::cout<<"BIND INCONSISTENCY "<<s.skeleton.bones[i].name<<" "<<d<<"\n";}
 auto&a=s.animations[0];std::cout<<"clip "<<a.sourceName<<" frames="<<a.durationFrames<<" fps="<<a.framerate<<" tracks="<<a.tracks.size()<<" unmapped="<<a.unmappedCurveCount<<"\n";
 auto p=s.sampleLocalPose(0,0);for(size_t i=0;i<s.skeleton.bones.size();++i){auto&b=s.skeleton.bones[i];if(!b.name.starts_with("pb2cast_weapon__"))continue;auto v=p[i];std::cout<<b.name<<" parent="<<b.parent<<" pos="<<v.position.x<<","<<v.position.y<<","<<v.position.z<<" scale="<<v.scale.x<<","<<v.scale.y<<","<<v.scale.z<<"\n";}
 for(auto&m:s.meshes)if(m.viewmodelWeapon){std::map<std::string,float> weights;for(auto&v:m.vertices)for(int k=0;k<4;++k)if(v.weights[k]>.001f)weights[s.skeleton.bones[v.bones[k]].name]+=v.weights[k];std::cout<<"MESH "<<m.name;for(auto&[b,w]:weights)std::cout<<" "<<b<<":"<<w;std::cout<<"\n";}
 for(float frame:{0.f,1.f,10.f,20.f}){auto a=s.samplePose(0,frame),b=s.samplePose(0,frame+.25f);float d=0;for(size_t i=0;i<a.size();++i)for(int k=0;k<16;++k)d=std::max(d,std::abs(a[i].v[k]-b[i].v[k]));std::cout<<"quarter-frame delta at "<<frame<<" = "<<d<<"\n";}
}
