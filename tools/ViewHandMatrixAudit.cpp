#include "assets/AssetCatalog.h"
#include "scene/ImportedViewRetarget.h"
#include <fstream>
#include <iostream>
#include <map>
#include <set>
int main(int argc,char** argv){
    if(argc<3)return 2;const std::filesystem::path root=argv[1],out=argv[2];std::filesystem::create_directories(out);
    std::ofstream report(out/"hands.txt"),bones(out/"unrecognized-bones.txt");std::string error;assets::Catalog catalog;
    for(const auto& dir:std::filesystem::directory_iterator(root))if(dir.is_directory()&&dir.path().filename().string().front()!='_'){
        const auto game=dir.path().filename().string();if(!assets::appendScan(dir.path(),game,catalog,error)){std::cerr<<error;return 2;}
    }
    std::map<std::string,scene::CastScene> representatives;std::vector<std::pair<std::string,scene::CastScene>> skins;
    std::size_t total{},invalid{};
    for(const auto& a:catalog.entries)if(a.role==assets::Role::ViewHands){
        auto doc=cast::Document::load(a.path);if(!doc.valid()){report<<"INVALID_DOCUMENT "<<a.game<<' '<<a.path<<'\n';++invalid;continue;}
        auto s=scene::buildScene(doc,false);const auto layout=scene::imported::viewLayout(s);++total;invalid+=!layout.usable();
        report<<(layout.usable()?"OK ":"UNRECOGNIZED ")<<a.game<<' '<<a.name<<" bones="<<s.skeleton.bones.size()<<" meshes="<<s.meshes.size();
        for(const auto& arm:layout.arms){report<<" arm="<<arm.shoulder<<','<<arm.elbow<<','<<arm.wrist;for(const auto& finger:arm.fingers)report<<" digit="<<finger[0]<<','<<finger[1]<<','<<finger[2];}report<<'\n';
        if(!layout.usable()){bones<<"GAME "<<a.game<<' '<<a.path<<'\n';for(const auto& bone:s.skeleton.bones)bones<<bone.name<<" parent="<<bone.parent<<" pos="<<bone.restGlobal.v[12]<<','<<bone.restGlobal.v[13]<<','<<bone.restGlobal.v[14]<<'\n';}
        if(layout.usable()){if(!representatives.contains(a.game))representatives.emplace(a.game,s);skins.emplace_back(a.game+"/"+a.name,std::move(s));}
    }
    report<<"TOTAL "<<total<<" UNRECOGNIZED "<<invalid<<std::endl;std::cout<<"Hands "<<total<<", unrecognized "<<invalid<<std::endl;
    if(argc>3){std::ofstream pairs(out/"pairs.txt");std::size_t count{},fail{};
        for(const auto& [driverGame,base]:representatives)for(const auto& [skinGame,skin]:skins){auto driver=base;const auto before=driver.skeleton.bones.size();bool ok=scene::imported::fitForeignViewSkin(driver,skin,error)&&driver.skeleton.bones.size()==before;for(const auto& m:driver.meshes)for(const auto& v:m.vertices)ok&=std::isfinite(v.position.x)&&std::isfinite(v.position.y)&&std::isfinite(v.position.z);++count;fail+=!ok;pairs<<(ok?"OK ":"FAIL ")<<skinGame<<" -> "<<driverGame<<" "<<error<<'\n';}
        pairs<<"TOTAL "<<count<<" FAILED "<<fail<<std::endl;std::cout<<"Pairs "<<count<<", failed "<<fail<<std::endl;
    }
    return 0;
}
