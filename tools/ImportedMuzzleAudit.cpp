#include "scene/CastScene.h"
#include "assets/ImportedGamePolicy.h"
#include <iostream>
int main(int argc,char** argv){
    if(argc==4&&std::string_view(argv[1])=="--motion"){
        auto s=scene::buildScene(cast::Document::load(argv[2]),false);scene::appendAnimations(cast::Document::load(argv[3]),s);
        if(s.animations.empty()||s.muzzleAnchors.empty())return 2;
        float travel{};scene::Vec3 first{};
        for(int frame=0;frame<=10;++frame){const auto pose=s.samplePose(0,s.animations[0].durationFrames*frame/10.f);
            const auto p=scene::resolveMuzzlePosition(s,pose);if(!p)return 1;if(frame==0)first=*p;travel=std::max(travel,scene::length(*p-first));
            for(const auto& anchor:s.muzzleAnchors){const auto side=scene::resolveMuzzlePosition(s,pose,-1,anchor.side);if(!side)return 1;}
        }
        std::cout<<"anchors="<<s.muzzleAnchors.size()<<" sampled=11 muzzle travel="<<travel<<'\n';return 0;
    }
    if(argc==3&&std::string_view(argv[1])=="--scan"){
        int models{},resolved{},missing{};
        for(const auto& entry:std::filesystem::recursive_directory_iterator(argv[2])){
            if(!entry.is_regular_file()||entry.path().extension()!=".cast")continue;
            const auto game=entry.path().parent_path().parent_path().filename().string();
            const auto name=assets::imported::lower(entry.path().stem().string());
            const auto role=assets::imported::role(game,name);
            if(role!=assets::Role::ViewWeapon&&role!=assets::Role::WorldWeapon)continue;
            if(name.find("knife")!=std::string::npos||name.find("grenade")!=std::string::npos||name.find("bomb")!=std::string::npos||name.find("sword")!=std::string::npos)continue;
            auto s=scene::buildScene(cast::Document::load(entry.path()),false);++models;
            std::vector<scene::Mat4> pose;for(const auto& b:s.skeleton.bones)pose.push_back(b.restGlobal);
            if(scene::resolveMuzzlePosition(s,pose))++resolved;
            else{++missing;std::cout<<"MISSING "<<entry.path().string()<<'\n';}
        }
        std::cout<<"models="<<models<<" resolved="<<resolved<<" missing="<<missing<<'\n';return missing?1:0;
    }
    for(int i=1;i<argc;++i){
        auto s=scene::buildScene(cast::Document::load(argv[i]),false);
        std::cout<<argv[i]<<" bones="<<s.skeleton.bones.size()<<" scale="<<s.importedTranslationScale<<'\n';
        for(const auto& b:s.skeleton.bones)std::cout<<b.name<<" p="<<b.restGlobal.v[12]<<','<<b.restGlobal.v[13]<<','<<b.restGlobal.v[14]<<" parent="<<b.parent<<'\n';
        if(s.bounds.valid)std::cout<<"bounds="<<s.bounds.minimum.x<<','<<s.bounds.minimum.y<<','<<s.bounds.minimum.z<<" to "<<s.bounds.maximum.x<<','<<s.bounds.maximum.y<<','<<s.bounds.maximum.z<<'\n';
        std::vector<scene::Mat4> pose;for(const auto& b:s.skeleton.bones)pose.push_back(b.restGlobal);
        const auto p=scene::resolveMuzzlePosition(s,pose);
        if(p)std::cout<<"resolved="<<p->x<<','<<p->y<<','<<p->z<<'\n';else std::cout<<"unresolved\n";
    }
}
