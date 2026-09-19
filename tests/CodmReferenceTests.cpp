#include "app/CodmReferenceHands.h"
#include "scene/CodmNative.h"
#include "scene/CodmLegacyAdapter.h"
#include <iostream>
#include <stdexcept>
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){try{
    assets::Catalog catalog;
    const auto add=[&](std::string name,assets::Role role=assets::Role::ViewHands,std::string game="codm"){
        assets::Asset a;a.name=name;a.role=role;a.game=game;catalog.entries.push_back(a);
    };
    add("codm_viewhands_C_M_Ghost_classic_CN_1P");
    add("codm_viewhands_C_M_Ghost_1P",assets::Role::PlayerModel);
    add("codm_viewhands_C_M_Ghost_1P",assets::Role::ViewHands,"bo2");
    check(!cadence::codm::referenceHands(catalog),"incorrect reference accepted");
    add("codm_viewhands_c_m_ghost_default");
    check(cadence::codm::referenceHands(catalog)==3,"legacy alias missing");
    add("viewhands_C_M_Ghost_1P");
    check(cadence::codm::referenceHands(catalog)==4,"unprefixed alias missing");
    add("codm_viewhands_C_M_Ghost_1P",assets::Role::ViewHands,"CODM");
    check(cadence::codm::referenceHands(catalog)==5,"current reference not preferred");
    std::reverse(catalog.entries.begin(),catalog.entries.end());
    check(cadence::codm::referenceHands(catalog)==0,"reference depends on ordering");
    if(argc==3){
        const std::filesystem::path root=argv[1];
        catalog.entries.clear();
        for(const auto& f:std::filesystem::directory_iterator(root/"models")){
            if(f.path().extension()!=".cast")continue;
            assets::Asset a;a.name=f.path().stem().string();a.path=f.path();a.game="codm";
            a.role=a.name.starts_with("codm_viewhands_")?assets::Role::ViewHands:assets::Role::OtherModel;
            catalog.entries.push_back(a);
        }
        const auto found=cadence::codm::referenceHands(catalog);
        check(found.has_value(),"installed reference not found");
        const auto hands=cast::Document::load(catalog.entries[*found].path);
        const auto legacy=cast::Document::load(argv[2]);
        check(hands.valid()&&legacy.valid(),"hand document invalid");
        auto reference=scene::buildScene(hands);
        scene::codm::normalizeToCentimetres(reference,scene::codm::presentationScale);
        for(const char* name:{"viewmodel_special_raygun","viewmodel_ar_ak47","viewmodel_smg_pdw57_TWD"}){
            scene::CastScene model;std::string error;
            if(!scene::codm::assemble(cast::Document::load(root/"models"/(std::string(name)+".cast")),hands,model,error))
                throw std::runtime_error(error);
            check(!model.meshes.empty()&&model.meshes.front().specularGlossiness,"native specular/gloss workflow missing");
            if(!scene::codm::fitLegacyHands(model,reference,legacy,error))
                throw std::runtime_error(error);
            check(!model.meshes.empty(),"empty assembled scene");
            const auto pose=model.samplePose(0,0);
            for(const auto& m:pose)for(float v:m.v)check(std::isfinite(v),"nonfinite fitted pose");
            std::cout<<name<<": native assembly + BO2 fit passed; meshes="<<model.meshes.size()<<"\n";
        }
    }
    std::cout<<"CODM reference-hand tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
