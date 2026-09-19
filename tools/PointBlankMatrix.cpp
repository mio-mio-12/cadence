#include "scene/PointBlankAdapter.h"
#include "app/PointBlankAnimationPolicy.h"
#include "assets/AssetCatalog.h"
#include <iostream>
#include <fstream>
int main(int argc,char**argv){
 if(argc<3)return 2;std::filesystem::path root=argv[1];assets::Catalog c;std::string error;assets::appendScan(root/"pointblank","pointblank",c,error);
 std::vector<assets::Asset> hands,weapons;for(auto&a:c.entries){if(a.role==assets::Role::ViewHands)hands.push_back(a);if(a.role==assets::Role::ViewWeapon)weapons.push_back(a);}
 std::ofstream out(argv[2]);out<<"weapons="<<weapons.size()<<" hands="<<hands.size()<<"\n";int failures=0;
 for(auto&hand:hands)for(auto&weapon:weapons){scene::CastScene s;if(!scene::pointblank::assemble(cast::Document::load(weapon.path),cast::Document::load(hand.path),s,error)){out<<"FAIL "<<error<<"\n";++failures;continue;}
 for(auto& f:std::filesystem::recursive_directory_iterator(root/"pointblank/animations/viewmodel")){const auto n=f.path().filename().string();if(n.starts_with(weapon.name+"_")&&n.ends_with(".cast")&&n.find("_Weapon _ ")==std::string::npos)scene::appendAnimations(cast::Document::load(f.path()),s);}
 cadence::pointblank_actions::prepare(s);size_t samples=0,missing=0;
 for(size_t a=0;a<s.animations.size();++a){if(s.animations[a].tracks.empty()){++failures;++missing;continue;}for(int i=0;i<=16;++i){auto p=s.samplePose(a,s.animations[a].durationFrames*i/16.f);++samples;for(auto&m:p)for(auto v:m.v)if(!std::isfinite(v))++failures;}}
 out<<hand.name<<" / "<<weapon.name<<" clips="<<s.animations.size()<<" samples="<<samples<<" empty="<<missing<<"\n";
 if(hand.name!="viewmodel_SWAT_Male_hands")continue;
 for(auto game:{"bo2","mw","mw3","ghosts","aw","iw_sp","mwr"}){assets::Catalog target;assets::appendScan(root/game,game,target,error);auto it=std::find_if(target.entries.begin(),target.entries.end(),[](auto&a){return a.role==assets::Role::ViewHands;});if(it==target.entries.end())continue;auto adapted=s;
 if(!scene::pointblank::fitHands(adapted,cast::Document::load(hand.path),cast::Document::load(it->path),error)){out<<"UNSUPPORTED "<<game<<" "<<it->name<<" : "<<error<<"\n";continue;}
 float maxError=0,wristError=0;for(size_t a=0;a<s.animations.size();++a)for(int i=0;i<=4;++i){float f=s.animations[a].durationFrames*i*.25f;auto src=s.samplePose(a,f),dst=adapted.samplePose(a,f);for(auto&m:dst)for(float v:m.v)if(!std::isfinite(v))++failures;for(size_t b=0;b<src.size();++b)for(int k=0;k<16;++k)maxError=std::max(maxError,std::abs(src[b].v[k]-dst[b].v[k]));for(auto pair:{std::pair{"L Hand","codm_legacy|j_wrist_le"},std::pair{"R Hand","codm_legacy|j_wrist_ri"}}){const auto sb=s.skeleton.boneByName.at(pair.first),db=adapted.skeleton.boneByName.at(pair.second);wristError=std::max(wristError,scene::length(scene::transformPoint(src[sb],{})-scene::transformPoint(dst[db],{})));}}
 out<<"ADAPTER "<<game<<" "<<it->name<<" sourceMatrixError="<<maxError<<" wristErrorCm="<<wristError<<" bones="<<adapted.skeleton.bones.size()<<"\n";if(maxError>1e-5||wristError>.5f)++failures;
 }
 }out<<"failures="<<failures<<"\n";std::cout<<"failures="<<failures<<"\n";return failures?1:0;
}
