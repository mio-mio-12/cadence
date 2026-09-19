#include "scene/CastScene.h"
#include "scene/PointBlankNative.h"
#include <iostream>
#include <set>
int main(int argc,char**argv){
 if(argc<2)return 1;
 auto d=cast::Document::load(argv[1]);auto s=scene::buildScene(d,false);
 if(argc>3){std::string e;if(!scene::pointblank::assemble(d,cast::Document::load(argv[2]),s,e)){std::cerr<<e;return 2;}scene::appendAnimations(cast::Document::load(argv[3]),s);}
 std::cout<<"valid="<<d.valid()<<" meshes="<<s.meshes.size()<<" bones="<<s.skeleton.bones.size()<<" animations="<<s.animations.size()<<"\n";
 for(size_t i=0;i<s.skeleton.bones.size();++i){auto&b=s.skeleton.bones[i];auto p=scene::transformPoint(b.restGlobal,{});std::cout<<i<<" "<<b.name<<" parent="<<b.parent<<" position="<<p.x<<","<<p.y<<","<<p.z<<"\n";}
 for(auto&m:s.meshes){std::cout<<"mesh "<<m.name<<" vertices="<<m.vertices.size()<<" triangles="<<m.indices.size()/3<<" skinned="<<m.skinned<<"\n";scene::Vec3 lo{1e9f,1e9f,1e9f},hi{-1e9f,-1e9f,-1e9f};std::set<unsigned> drivers;for(auto&v:m.vertices){auto p=scene::transformPoint(m.modelTransform,v.position);lo.x=std::min(lo.x,p.x);lo.y=std::min(lo.y,p.y);lo.z=std::min(lo.z,p.z);hi.x=std::max(hi.x,p.x);hi.y=std::max(hi.y,p.y);hi.z=std::max(hi.z,p.z);for(int k=0;k<4;++k)if(v.weights[k]>0)drivers.insert(v.bones[k]);}std::cout<<" bounds "<<lo.x<<","<<lo.y<<","<<lo.z<<" -> "<<hi.x<<","<<hi.y<<","<<hi.z<<" drivers";for(auto b:drivers)std::cout<<" "<<b<<":"<<(b<s.skeleton.bones.size()?s.skeleton.bones[b].name:"invalid");std::cout<<"\n";}
 for(auto&a:s.animations)std::cout<<"anim "<<a.name<<" fps="<<a.framerate<<" frames="<<a.durationFrames<<" tracks="<<a.tracks.size()<<"\n";
 if(argc>3&&!s.animations.empty()){auto pose=s.samplePose(0,0);for(size_t i=0;i<pose.size();++i){auto p=scene::transformPoint(pose[i],{});std::cout<<"pose "<<s.skeleton.bones[i].name<<" "<<p.x<<","<<p.y<<","<<p.z<<"\n";}}
 auto visit=[&](auto&&self,const cast::Node&n)->void{auto t=cast::nodeTypeName(n.identifier);if(t!="Mesh"&&t!="Bone"&&t!="Skeleton") {std::cout<<"node "<<t<<"\n";for(auto&p:n.properties)std::cout<<"  "<<p.name<<"="<<d.propertyPreview(p,4)<<"\n";}for(auto&c:n.children)self(self,c);};
 if(argc==3)for(auto&r:d.roots())visit(visit,r);
 for(auto&w:s.warnings)std::cout<<"WARNING "<<w<<"\n";
}
