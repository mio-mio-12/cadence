// Called once at rig assembly, before mapping/retargeting clips. Always infer
// from native T6 hands; the selected hands must not alter weapon mechanisms.
void calibrateT6Magazine(AppState& app,const assets::Asset* selectedWeapon=nullptr){
    if(!scene::hasSeparateT6Magazine(app.scene))return;
    const assets::Asset* weapon=selectedWeapon;
    if(!weapon)for(const auto& path:app.loadedRigModelPaths){
        for(const auto& asset:app.assetCatalog.entries)
            if(asset.path==path&&lowerText(asset.name).starts_with("t6_wpn_")&&lowerText(asset.name).find("_view")!=std::string::npos){weapon=&asset;break;}
        if(weapon)break;
    }
    if(!weapon)return;
    const auto prefix=animationPrefixForWeapon(*weapon);if(prefix.empty())return;
    std::filesystem::path hands;
    for(const auto& asset:app.assetCatalog.entries)if(lowerText(asset.game)=="bo2"&&asset.role==assets::Role::ViewHands){hands=asset.path;break;}
    if(hands.empty()){app.scene.warnings.push_back("T6 magazine: native hands unavailable; missing socket cannot be recovered.");return;}
    std::vector<const cast::Document*> clips;
    for(const auto& doc:app.animationDocuments){const auto file=lowerText(std::filesystem::path(doc.sourceName()).filename().string());
        if(file==prefix+"reload.cast"||file==prefix+"reload_empty.cast")clips.push_back(&doc);}
    if(clips.empty()){app.scene.warnings.push_back("T6 magazine: native reloads unavailable; missing socket cannot be recovered.");return;}
    std::string key;
    const auto identify=[&](const std::filesystem::path& path){std::error_code ec;const auto size=std::filesystem::file_size(path,ec);const auto time=std::filesystem::last_write_time(path,ec);key+=path.string()+":"+std::to_string(size)+":"+std::to_string(time.time_since_epoch().count())+"|";};
    identify(hands);for(const auto& path:app.loadedRigModelPaths)identify(path);for(auto doc:clips)identify(std::filesystem::u8path(doc->sourceName()));
    static std::unordered_map<std::string,std::optional<scene::T6MagazineCalibration>> cache;
    auto found=cache.find(key);
    if(found==cache.end()){
        auto native=scene::buildScene(cast::Document::load(hands));
        for(const auto& path:app.loadedRigModelPaths)scene::appendRigModel(cast::Document::load(path),native,path.stem().string());
        for(auto doc:clips)scene::appendAnimations(*doc,native);
        const auto result=scene::inferT6MagazineMount(native);
        if(cache.size()>=128)cache.clear();found=cache.emplace(std::move(key),result).first;
    }
    if(found->second)scene::applyT6MagazineMount(app.scene,*found->second);
    else app.scene.warnings.push_back("T6 magazine: insufficient native reload contact to recover missing attachment socket.");
}
