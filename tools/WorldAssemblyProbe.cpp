#include "scene/CastScene.h"
#include "scene/NativeBoneNames.h"
#include <iostream>
#include <set>
#include <functional>
int main(int argc,char** argv){
 if(argc<2)return 1;
 auto doc=cast::Document::load(argv[1]);auto s=scene::buildScene(doc,false);
 for(auto& m:s.meshes){scene::Vec3 lo{1e9f,1e9f,1e9f},hi{-1e9f,-1e9f,-1e9f};for(auto& v:m.vertices){lo.x=std::min(lo.x,v.position.x);lo.y=std::min(lo.y,v.position.y);lo.z=std::min(lo.z,v.position.z);hi.x=std::max(hi.x,v.position.x);hi.y=std::max(hi.y,v.position.y);hi.z=std::max(hi.z,v.position.z);}std::cout<<"MESH "<<m.materialName<<" bounds "<<lo.x<<","<<lo.y<<","<<lo.z<<" -> "<<hi.x<<","<<hi.y<<","<<hi.z<<"\n";}
 for(auto& b:s.skeleton.bones)std::cout<<"BONE "<<b.name<<" parent="<<b.parent<<" pos="<<b.restGlobal.v[12]<<","<<b.restGlobal.v[13]<<","<<b.restGlobal.v[14]<<"\n";
 if(argc>2){auto anim=cast::Document::load(argv[2]);std::set<std::string> missing;std::function<void(const cast::Node&)> scan=[&](const auto& n){if(auto p=n.findProperty("nn");p&&p->stringValue&&!s.skeleton.boneByName.contains(*p->stringValue)&&!s.skeleton.boneByCanonicalName.contains(scene::canonicalName(*p->stringValue))){missing.insert(*p->stringValue);std::cout<<"CURVE "<<*p->stringValue;for(auto& prop:n.properties)std::cout<<" "<<prop.name<<"="<<anim.propertyPreview(prop,8);std::cout<<"\n";}for(auto& c:n.children)scan(c);};for(auto& r:anim.roots())scan(r);for(auto& n:missing)std::cout<<"UNMAPPED "<<n<<"\n";
  scene::appendAnimations(anim,s);for(size_t i=0;i<s.animations.size();++i){const auto& clip=s.animations[i];size_t owned=0;for(auto&t:clip.tracks)owned+=t.ownsLayer;std::cout<<"ACTION domain="<<int(clip.domain)<<" action="<<int(clip.action)<<" stance="<<int(clip.stance)<<" weapon="<<int(clip.weapon)<<" owned="<<owned<<" tracks="<<clip.tracks.size()<<"\n";for(int f=0;f<=4;++f)for(auto&m:s.samplePose(i,clip.durationFrames*f/4.f))for(float value:m.v)if(!std::isfinite(value))return 2;}
  for(int i=3;i<argc;++i){auto donor=scene::buildScene(cast::Document::load(argv[i]),false);for(auto& b:donor.skeleton.bones)if(missing.contains(scene::nativeBoneHash(b.name)))std::cout<<"IDENTIFIED "<<b.name<<" "<<scene::nativeBoneHash(b.name)<<"\n";}
 }
}
