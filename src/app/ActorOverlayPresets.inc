// Standalone overlays: loading a look never changes scene lighting or gameplay.
template<class App> auto actorInspectorValues(App& app){
    return std::tie(app.debugWallhack,app.wallhackVisibleColor.x,app.wallhackVisibleColor.y,app.wallhackVisibleColor.z,
        app.wallhackOccludedColor.x,app.wallhackOccludedColor.y,app.wallhackOccludedColor.z,app.wallhackThickness,
        app.debugHitboxes,app.hitboxColor.x,app.hitboxColor.y,app.hitboxColor.z,app.hitboxThickness);
}
bool saveActorOverlayPreset(const AppState& app,const std::filesystem::path& path){
    std::error_code error;std::filesystem::create_directories(path.parent_path(),error);
    if(error)return false;
    std::ofstream out(path);out.imbue(std::locale::classic());out<<std::setprecision(9)<<"CADENCE_ACTOR_OVERLAYS 1\n";
    writeVisualValues(out,actorOverlayValues(app));writeVisualValues(out,actorInspectorValues(app));
    out.flush();return static_cast<bool>(out);
}
bool loadActorOverlayPreset(AppState& app,const std::filesystem::path& path){
    std::ifstream in(path);in.imbue(std::locale::classic());std::string signature;int version=0;
    if(!(in>>signature>>version)||signature!="CADENCE_ACTOR_OVERLAYS"||version!=1)return false;
    // Parse into small staging state; malformed files cannot partly apply.
    struct Values {
        decltype(app.actorOverlays) actorOverlays;
        bool debugWallhack{},debugHitboxes{};
        scene::Vec3 wallhackVisibleColor,wallhackOccludedColor,hitboxColor;
        float wallhackThickness{},hitboxThickness{};
    } value;
    if(!readVisualValues(in,actorOverlayValues(value))||!readVisualValues(in,actorInspectorValues(value)))return false;
    value.actorOverlays.sanitize();
    value.wallhackThickness=std::clamp(value.wallhackThickness,.5f,8.f);
    value.hitboxThickness=std::clamp(value.hitboxThickness,.5f,8.f);
    actorOverlayValues(app)=actorOverlayValues(value);actorInspectorValues(app)=actorInspectorValues(value);
    return true;
}
void drawActorOverlayPresets(AppState& app){
    if(!ImGui::TreeNode("Presets##actor_overlay_presets"))return;
    static std::array<char,128> name{};
    static std::string selected;
    static std::vector<std::filesystem::path> files;
    const auto folder=cadenceAssetsDirectory()/"actor_overlay_presets";
    // Scan on opening the picker, not on every rendered frame.
    if(ImGui::BeginCombo("Saved presets##actor_overlay",selected.empty()?"Choose preset":selected.c_str())){
        if(ImGui::IsWindowAppearing()){
            files.clear();std::error_code error;
            for(std::filesystem::directory_iterator it(folder,error),end;!error&&it!=end;it.increment(error))
                if(it->is_regular_file(error)&&it->path().extension()==".castoverlay")files.push_back(it->path());
            std::sort(files.begin(),files.end());
        }
        for(const auto& path:files)if(ImGui::Selectable(path.stem().string().c_str(),selected==path.stem().string())){
            if(loadActorOverlayPreset(app,path)){selected=path.stem().string();app.status="Loaded actor overlays: "+selected;}
            else app.status="Could not read actor overlay preset";
        }
        ImGui::EndCombo();
    }
    ImGui::InputTextWithHint("Name##actor_overlay","Preset name",name.data(),name.size());
    if(ImGui::Button("Save preset##actor_overlay")){
        selected=safePackageId(name.data());
        app.status=saveActorOverlayPreset(app,folder/(selected+".castoverlay"))?"Saved actor overlays: "+selected:"Could not save actor overlay preset";
    }
    ImGui::TreePop();
}
