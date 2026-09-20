#include "assets/LocalAssetPaths.h"
#include "assets/AssetCatalog.h"
#include "assets/CharacterParts.h"
#include "scene/PointBlankNative.h"
#include <iostream>
#include <map>
int main(){
 assets::Catalog c;std::string error;
 if(!assets::appendScan(cadence::local_assets::exportPath("pointblank"),"pointblank",c,error))return 1;
 std::map<std::string,int> pairs;int failures=0,bodies=0,hands=0;
 for(const auto&a:c.entries){
  if(a.path.parent_path().filename()!="models")continue;
  if(a.role!=assets::Role::PlayerModel&&a.role!=assets::Role::ViewHands)continue;
  const auto id=assets::character::pointBlankIdentity(a.name);auto s=scene::buildScene(cast::Document::load(a.path));
  const bool body=a.role==assets::Role::PlayerModel;
  const bool valid=!s.meshes.empty()&&s.skeleton.boneByName.contains("R Hand")&&s.skeleton.boneByName.contains("L Hand")&&(!body||scene::pointblank::body(s.skeleton));
  failures+=!valid;pairs[id]|=body?1:2;body?++bodies:++hands;
  std::cout<<a.name<<" | "<<assets::character::pointBlankFaction(a.name)<<" | valid="<<valid<<"\n";
 }
 for(const auto&[id,mask]:pairs)if(mask!=3){++failures;std::cout<<"UNPAIRED "<<id<<"\n";}
 std::cout<<"bodies="<<bodies<<" hands="<<hands<<" pairs="<<pairs.size()<<" failures="<<failures<<"\n";
 return failures?1:0;
}
