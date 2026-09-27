#include "assets/LocalAssetPaths.h"
#define main cadenceApplicationMain
#include "../src/app/main.cpp"
#undef main
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<'\n';return 1;}}while(false)
int main(){
    auto state=std::make_unique<AppState>();auto& a=*state;std::string error;
    const std::filesystem::path root=cadence::local_assets::exportPath("");
    a.weaponSunMultiplier=2.25f;a.weaponAmbientMultiplier=.125f;
    CHECK(saveVisualPreset(a,"diagnostics/v225/lighting.castvisual"));
    auto target=std::make_unique<AppState>();VisualPresetResources resources;
    std::ifstream in("diagnostics/v225/lighting.castvisual");
    CHECK(parseVisualPreset(*target,in,resources));
    CHECK(target->weaponSunMultiplier==2.25f&&target->weaponAmbientMultiplier==.125f);
    copyVisualPresetSettings(a,*target);
    CHECK(a.weaponSunMultiplier==2.25f);
    std::istringstream legacy("CASTVISUAL 5\n");CHECK(parseVisualPreset(a,legacy,resources));
    CHECK(a.weaponSunMultiplier==1.f&&a.weaponAmbientMultiplier==1.f);
    CHECK(assets::appendScan(root/"iw_sp","iw_sp",a.assetCatalog,error));
    auto idx=[&](const std::string& name){for(size_t i=0;i<a.assetCatalog.entries.size();++i)if(a.assetCatalog.entries[i].name==name)return i;return a.assetCatalog.entries.size();};
    const auto hands=idx("viewmodel_base_viewhands_iw7_LOD0");
    const auto gun=idx("weapon_kb_m4_vm_fallback_LOD0");CHECK(hands<a.assetCatalog.entries.size()&&gun<a.assetCatalog.entries.size());
    a.selectedBaseAsset=hands;a.selectedWeaponAsset=gun;
    a.scene=scene::buildScene(cast::Document::load(a.assetCatalog.entries[hands].path));
    a.loadedRigModelPaths={a.assetCatalog.entries[gun].path};
    scene::appendRigModel(cast::Document::load(a.loadedRigModelPaths.front()),a.scene,"weapon");
    a.scene.viewHandsDriverGame="iw_sp";a.gameplayWeapon=scene::WeaponClass::Rifle;
    a.weaponProfile=weapon::makeGenerated("kb_m4",weapon::Archetype::Rifle,"vm_kb_m4_");
    const auto folder=root/"iw_sp/animations/vm";
    scene::appendAnimations(cast::Document::load(folder/"vm_kb_m4_idle.cast"),a.scene);
    scene::appendAnimations(cast::Document::load(folder/"vm_default_sprint_loop_medium.cast"),a.scene);
    installIwSharedSprint(a,a.assetCatalog.entries[gun]);
    for(const auto slot:{"sprint_in","sprint_loop","sprint_out"}){
        const auto clip=findViewmodelClip(a,slot);CHECK(clip);
        const auto& anim=a.scene.animations[*clip];CHECK(anim.tracks.size()>100);
        const auto first=a.scene.samplePose(*clip,0),middle=a.scene.samplePose(*clip,anim.durationFrames*.5f);
        bool moved=false;for(size_t b=0;b<first.size();++b)for(int k=0;k<16;++k){CHECK(std::isfinite(middle[b].v[k]));moved|=std::abs(first[b].v[k]-middle[b].v[k])>.01f;}CHECK(moved);
        std::cout<<slot<<" tracks="<<anim.tracks.size()<<" frames="<<anim.durationFrames<<'\n';
    }
    a.weaponProfile.animations["sprint_loop"]="vm_kb_m4_idle.cast";a.weaponProfile.animationVariants.erase("sprint_loop");
    installIwSharedSprint(a,a.assetCatalog.entries[gun]);
    const auto selected=findViewmodelClip(a,"sprint_loop");CHECK(selected&&a.scene.animations[*selected].sourceName=="vm_kb_m4_idle.cast");
    std::cout<<"IW sprint grip, override persistence and lighting preset checks passed\n";
}
