#include "scene/ImportedBody.h"
#include "assets/AssetCatalog.h"
#include <iostream>
int main(int argc,char** argv){
    if(argc!=2)return 2;assets::Catalog catalog;std::string error;
    if(!assets::scan(argv[1],catalog,error)){std::cerr<<error;return 3;}
    scene::CastScene base;for(const auto& a:catalog.entries)if(a.role==assets::Role::PlayerModel&&a.name.starts_with("SK_VC06")){base=scene::buildScene(cast::Document::load(a.path),false);break;}
    if(base.meshes.empty())return 4;
    std::size_t total{},oldAccepted{},newAccepted{},failures{};float maximumRestError{};
    for(const auto& a:catalog.entries){if(a.role!=assets::Role::PlayerModel)continue;
        const auto doc=cast::Document::load(a.path);const auto native=scene::buildScene(doc);++total;
        oldAccepted+=scene::imported::sharedBodyAssemblyCompatible(base,native);
        auto assembled=base;assembled.meshes.clear();const auto added=scene::appendRigModel(doc,assembled,a.name);
        if(added!=native.meshes.size()){++failures;continue;}++newAccepted;
        const auto pose=assembled.samplePose(static_cast<std::size_t>(-1),0);
        for(std::size_t m=0;m<native.meshes.size();++m){const auto& original=native.meshes[m];const auto& result=assembled.meshes[m];
            if(original.vertices.size()!=result.vertices.size()){++failures;continue;}
            for(std::size_t i=0;i<original.vertices.size();++i){const auto& v=result.vertices[i];scene::Vec3 point{};float weight{};
                if(!result.skinned)point=v.position;else for(std::size_t k=0;k<v.bones.size();++k)if(v.weights[k]>0&&v.bones[k]<pose.size()){point+=scene::transformPoint(pose[v.bones[k]]*assembled.skeleton.bones[v.bones[k]].inverseBind,v.position)*v.weights[k];weight+=v.weights[k];}
                if(result.skinned&&weight>0)point=point/weight;
                const auto e=scene::length(point-original.vertices[i].position);maximumRestError=std::max(maximumRestError,e);if(!std::isfinite(e)||e>.05f){++failures;break;}
            }
        }
    }
    std::cout<<"models="<<total<<" old_pool="<<oldAccepted<<" assembled="<<newAccepted<<" failures="<<failures<<" maximum_rest_error="<<maximumRestError<<'\n';return failures?1:0;
}
