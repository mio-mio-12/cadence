// Integration coverage for the actual application profile resolution order.
#include <stdexcept>
int coldWarProfileRegression(const std::filesystem::path& exported,const std::filesystem::path& savedDefaults){
    auto storage=std::make_unique<AppState>();auto& app=*storage;
    std::string error;int failures=0;const auto check=[&](bool ok,const std::string& text){std::cout<<(ok?"PASS ":"FAIL ")<<text<<'\n';failures+=!ok;};
    if(!assets::appendScan(exported/"bocw_sp","bocw_sp",app.assetCatalog,error)){std::cerr<<error<<'\n';return 2;}
    std::vector<assets::Asset> variants;
    for(const auto& asset:app.assetCatalog.entries)if(asset.role==assets::Role::ViewWeapon&&assets::coldWarWeaponFamily(asset.name)=="sniper_standard")variants.push_back(asset);
    if(variants.empty()){std::cerr<<"No exported sniper_standard models\n";return 2;}
    const auto base=coldWarBaseWeaponAsset(app,variants.front());
    check(base&&base->name=="wpn_t9_sniper_standard_view_LOD0","actual Base selection is the suffix-free exported model");
    if(!base)return 2;
    const auto baseFile=savedDefaults/base->game/weaponDefaultProfilePath(*base).filename();weapon::Profile saved;
    if(!weapon::load(baseFile,saved,error)){std::cerr<<"Need real saved base profile: "<<baseFile<<": "<<error<<'\n';return 2;}
    const auto matches=[&](const weapon::Profile& candidate,const weapon::Profile& plate){
        if(candidate.stats.fireTime!=plate.stats.fireTime||candidate.stats.reloadTime!=plate.stats.reloadTime||candidate.stats.adsIn!=plate.stats.adsIn||candidate.stats.rechamberStartDelay!=plate.stats.rechamberStartDelay||candidate.stats.adsKickPitchMin!=plate.stats.adsKickPitchMin||candidate.stats.adsKickPitchMax!=plate.stats.adsKickPitchMax||candidate.stats.boltAction!=plate.stats.boltAction)return false;
        for(const auto& [key,value]:plate.animations)if(!candidate.animations.contains(key)||candidate.animations.at(key)!=value)return false;
        return candidate.animationVariants==plate.animationVariants;
    };
    for(const auto& asset:variants)check(matches(generatedWeaponProfile(app,asset,savedDefaults),saved),asset.name+" inherits actual saved base recoil/timing/selections");
    const auto temp=std::filesystem::temp_directory_path()/("cadence_cw_profile_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto fixturePath=[&](const std::filesystem::path& p){return temp/p.parent_path().filename()/p.filename();};
    const auto write=[&](const weapon::Profile& p,const std::filesystem::path& path){std::filesystem::create_directories(path.parent_path());if(!weapon::save(p,path,error))throw std::runtime_error(error);};
    auto plate=saved;plate.stats.fireTime=.217f;plate.stats.boltAction=false;plate.animations["fire"]="chosen_crossgame_fire.cast";plate.animationVariants["inspect"]={"chosen_inspect.cast","second_inspect.cast"};
    write(plate,fixturePath(weaponDefaultProfilePath(*base)));
    auto stale=plate;stale.stats.fireTime=4.f;stale.animations["fire"]="stale_family.cast";write(stale,fixturePath(weaponFamilyProfilePath(*base)));
    auto selected=variants.front();for(const auto& asset:variants)if(assets::coldWarWeaponVariant(asset.name)=="beachcomber")selected=asset;
    auto cosmetic=stale;cosmetic.name=selected.name;cosmetic.baseModel="variant_hands.cast";cosmetic.rigModels={"beachcomber_receiver.cast"};cosmetic.gunPosition={1,2,3};cosmetic.animationOffsets={{"chosen_crossgame_fire.cast",{4,5,6}}};write(cosmetic,fixturePath(weaponDefaultProfilePath(selected)));
    scene::Animation native;native.sourceName="vm_sniper_standard_t9_idle.cast";native.domain=scene::AnimationDomain::ViewModel;native.tracks.resize(1);app.scene.animations.push_back(native);
    auto resolved=generatedWeaponProfile(app,selected,temp);
    check(matches(resolved,plate),"base overrides stale family and cosmetic defaults after native discovery");
    check(resolved.name==selected.name&&resolved.baseModel==cosmetic.baseModel&&resolved.rigModels==cosmetic.rigModels&&resolved.gunPosition.x==1&&resolved.animationOffsets.front().position.x==4,"base behavior preserves selected cosmetic rig and fitting");
    populateDefaultDiscoveredAnimations(app,resolved,selected);
    check(matches(resolved,plate),"deferred discovery preserves base selections absent from loaded library and authored bolt flag");
    std::error_code ec;std::filesystem::remove_all(temp,ec);
    std::cout<<"Cold War profile integration: "<<variants.size()<<" actual models, "<<failures<<" failures\n";
    return failures?1:0;
}
