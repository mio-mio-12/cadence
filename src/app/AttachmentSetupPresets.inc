bool applyAttachmentSetup(AppState& app,const weapon::Profile& setup){
    if(setup.baseModel!=app.loadedBaseModelPath.string())return false;
    auto staged=app.scene;std::erase_if(staged.meshes,[](const auto& m){return m.attachmentIndex>=0;});staged.attachments.clear();std::vector<std::filesystem::path> paths;
    for(const auto& file:setup.rigModels){const auto path=std::filesystem::u8path(file);scene::CastScene imported;std::string error;
        if(lowerText(path.extension().string())==".glb"){scene::glb::Map map;if(!scene::glb::load(path,map,error))return false;imported=std::move(map.scene);}
        else {auto doc=cast::Document::load(path);if(!doc.valid())return false;imported=scene::buildScene(doc);}
        if(!scene::appendPreparedAttachment(std::move(imported),staged,0,path.stem().string()))return false;paths.push_back(path);
    }
    app.scene=std::move(staged);app.loadedAttachmentPaths=std::move(paths);app.weaponProfile.rigMounts=setup.rigMounts;app.weaponProfile.hiddenTags=setup.hiddenTags;applyWeaponRigState(app);app.weaponProfileDirty=true;return true;
}
std::filesystem::path attachmentSetupFolder(const assets::Asset& asset){return weaponDefaultProfilePath(asset).parent_path()/"attachment_setups"/asset.name;}
weapon::Profile currentAttachmentSetup(AppState& app){captureWeaponRigState(app);auto setup=app.weaponProfile;setup.rigModels.clear();for(const auto& p:app.loadedAttachmentPaths)setup.rigModels.push_back(p.string());return setup;}
void restoreDefaultAttachmentSetup(AppState& app,const assets::Asset& asset){
    const auto path=attachmentSetupFolder(asset)/"_default.iwweapon";std::error_code ec;if(!std::filesystem::is_regular_file(path,ec))return;weapon::Profile setup;std::string error;
    if(!weapon::load(path,setup,error)||!applyAttachmentSetup(app,setup))app.scene.warnings.push_back("Saved attachment default unavailable for this rig; existing setup retained");
}
void drawAttachmentSetupPresets(AppState& app){
    if(!ImGui::TreeNode("Saved attachment/tag setup"))return;
    const auto asset=activeWeaponAssetIndex(app);
    const auto folder=asset<app.assetCatalog.entries.size()?attachmentSetupFolder(app.assetCatalog.entries[asset]):cadenceAssetsDirectory()/"attachment_setups"/app.loadedBaseModelPath.stem();
    static std::filesystem::path previousFolder;static std::vector<std::filesystem::path> files;
    static std::array<char,128> name{};static std::string selected;static bool refresh=true;
    if(folder!=previousFolder){previousFolder=folder;selected.clear();name.fill(0);refresh=true;}
    if(refresh){files.clear();std::error_code ec;for(const auto& f:std::filesystem::directory_iterator(folder,ec))if(f.is_regular_file(ec)&&f.path().extension()==".iwweapon")files.push_back(f.path());std::sort(files.begin(),files.end());refresh=false;}
    if(ImGui::BeginCombo("Configuration",selected.empty()?"Choose saved configuration":selected.c_str())){
        for(const auto& path:files)if(ImGui::Selectable(path.stem().string().c_str(),selected==path.stem().string())){
            weapon::Profile setup;std::string error;
            if(!weapon::load(path,setup,error))app.status="Could not load setup: "+error;
            else if(setup.baseModel!=app.loadedBaseModelPath.string())app.status="Setup belongs to different viewhands; load those hands first";
            else if(!applyAttachmentSetup(app,setup))app.status="Setup could not be assembled; current setup unchanged";
            else {app.classGpuResident=false;uploadMainScene(app);selected=path.stem().string();std::snprintf(name.data(),name.size(),"%s",selected.c_str());app.status="Loaded attachment/tag setup "+selected;}
        }ImGui::EndCombo();
    }
    ImGui::InputText("Name",name.data(),name.size());
    if(ImGui::Button("Save")){
        std::string safe=name.data();for(auto& c:safe)if(!std::isalnum(static_cast<unsigned char>(c))&&c!='-'&&c!='_')c='_';
        if(safe.empty())app.status="Enter a configuration name";
        else {captureWeaponRigState(app);auto setup=app.weaponProfile;setup.rigModels.clear();for(const auto& p:app.loadedAttachmentPaths)setup.rigModels.push_back(p.string());std::error_code ec;std::filesystem::create_directories(folder,ec);std::string error;if(!ec&&weapon::save(setup,folder/(safe+".iwweapon"),error)){selected=safe;refresh=true;app.status="Saved attachment/tag setup "+safe;}else app.status="Setup save failed: "+(ec?ec.message():error);}
    }
    ImGui::SameLine();ImGui::BeginDisabled(asset>=app.assetCatalog.entries.size());
    if(ImGui::Button("Save as default")){const auto setup=currentAttachmentSetup(app);std::error_code ec;std::filesystem::create_directories(folder,ec);std::string error;if(!ec&&weapon::save(setup,folder/"_default.iwweapon",error)){app.weaponProfilePath=weaponDefaultProfilePath(app.assetCatalog.entries[asset]);autoSaveWeaponProfile(app);refresh=true;app.status="Saved default attachment/tag setup";}else app.status="Could not save attachment default: "+(ec?ec.message():error);}
    ImGui::EndDisabled();ImGui::SameLine();ImGui::BeginDisabled(selected.empty());
    if(ImGui::Button("Delete")){std::error_code ec;const auto path=folder/(selected+".iwweapon");if(std::filesystem::remove(path,ec)){app.status="Deleted saved setup "+selected+"; live setup unchanged";selected.clear();refresh=true;}else app.status="Could not delete setup: "+ec.message();}
    ImGui::EndDisabled();ImGui::TreePop();
}

void drawWeaponTags(AppState& app){
    if(!ImGui::TreeNode("Weapon tags"))return;
    bool changed=false;
    if(ImGui::Button("Enable all")){app.scene.hiddenBones.clear();changed=true;}
    if(ImGui::BeginChild("weapon_tag_checks",{0,180},ImGuiChildFlags_Borders))for(std::size_t i=0;i<app.scene.skeleton.bones.size();++i){
        const auto& bone=app.scene.skeleton.bones[i];const auto tag=lowerText(scene::canonicalName(bone.name));if(!tag.starts_with("tag_"))continue;
        bool enabled=!app.scene.hiddenBones.contains(i);ImGui::PushID(static_cast<int>(i));
        if(ImGui::Checkbox(bone.name.c_str(),&enabled)){if(enabled)app.scene.hiddenBones.erase(i);else app.scene.hiddenBones.insert(i);changed=true;}ImGui::PopID();
    }ImGui::EndChild();if(changed)captureWeaponRigState(app);ImGui::TreePop();
}
