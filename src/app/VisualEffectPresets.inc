// Shared serialization for visual presets and crosshair-only presets.
// Parse into value tuples first so a malformed record never partially applies.
template<class Tuple> void writeVisualValues(std::ostream& out,const Tuple& values){
    std::apply([&](const auto&... v){((out<<v<<' '),...);},values);out<<'\n';
}
template<class Tuple> bool readVisualValues(std::istream& in,Tuple refs){
    auto values=std::apply([](const auto&... v){return std::make_tuple(v...);},refs);
    std::apply([&](auto&... v){((in>>v),...);},values);
    if(!in)return false;
    const bool finite=std::apply([](const auto&... v){return (std::isfinite(static_cast<double>(v))&&...);},values);
    if(!finite)return false;
    refs=values;return true;
}
template<class App> auto crosshairValues(App& app){return std::tie(app.csgoClassicCrosshair,app.csgoCrosshairDynamic,app.csgoCrosshairCenterDot,app.csgoCrosshairScale,app.csgoCrosshairGap,app.csgoCrosshairLength,app.csgoCrosshairThickness,app.csgoCrosshairOutline,app.csgoCrosshairHorizontalSpacing,app.csgoCrosshairVerticalSpacing,app.csgoCrosshairSideMarks,app.csgoCrosshairMarkLength,app.csgoCrosshairMarkOffset,app.csgoCrosshairColor.x,app.csgoCrosshairColor.y,app.csgoCrosshairColor.z,app.csgoCrosshairColor.w,app.csgoCrosshairOutlineColor.x,app.csgoCrosshairOutlineColor.y,app.csgoCrosshairOutlineColor.z,app.csgoCrosshairOutlineColor.w);}
template<class App> auto weapon_flashValues(App& app){return std::tie(app.debugMuzzleFlash,app.muzzleFlashNotetracks,app.muzzleFlashSize,app.muzzleFlashDuration,app.muzzleFlashCurvePower,app.muzzleFlashColor.x,app.muzzleFlashColor.y,app.muzzleFlashColor.z,app.muzzleFlashColor.w,app.muzzleFlashRandomRotation,app.muzzleFlashRotationAngle);}
template<class App> auto weapon_smokeValues(App& app){return std::tie(app.smokeTrailEnabled,app.smokeTrailFeathering,app.smokeEmissionDuration,app.smokeCurveResolution,app.smokeTrailLifetime,app.smokeTrailStartWidth,app.smokeTrailEndWidth,app.smokeTrailTaperingExp,app.smokeTrailBlastSpeed,app.smokeTrailRiseSpeed,app.smokeTrailDispersion,app.smokeTrailColor.x,app.smokeTrailColor.y,app.smokeTrailColor.z,app.smokeTrailColor.w);}
template<class App> auto weapon_trailsValues(App& app){return std::tie(app.trailMuzzleFirstPct,app.trailMuzzleLastPct,app.sniperTrailEnabled,app.sniperTrailWidth,app.sniperTrailLifetime,app.sniperTrailFeathering,app.sniperTrailColor.x,app.sniperTrailColor.y,app.sniperTrailColor.z,app.sniperTrailEmissiveEnabled,app.sniperTrailEmissive,app.sniperTrailSprite,app.projectileTrailEnabled,app.projectileTrailSpeed,app.projectileTrailLength,app.projectileTrailWidth,app.projectileTrailColor.x,app.projectileTrailColor.y,app.projectileTrailColor.z,app.projectileTrailEmissiveEnabled,app.projectileTrailEmissive,app.projectileTrailSprite,app.bulletTrailEdgeDarkening,app.bulletTrailEdgePower,app.bulletTrailEdgeTint.x,app.bulletTrailEdgeTint.y,app.bulletTrailEdgeTint.z);}

bool saveCrosshairPreset(const AppState& app,const std::filesystem::path& path){
    std::error_code ec;std::filesystem::create_directories(path.parent_path(),ec);
    std::ofstream out(path);if(!out)return false;out<<std::setprecision(9)<<"CROSSHAIR 1\n";
    writeVisualValues(out,crosshairValues(app));return bool(out);
}
bool loadCrosshairPreset(AppState& app,const std::filesystem::path& path){
    std::ifstream in(path);std::string magic;int version{};
    return bool(in>>magic>>version)&&magic=="CROSSHAIR"&&version==1&&readVisualValues(in,crosshairValues(app));
}
void drawCrosshairPresets(AppState& app){
    static std::array<char,96> name{"custom"};
    static std::string selected;
    const auto directory=cadenceAssetsDirectory()/"crosshairs";
    if(ImGui::BeginCombo("Preset",selected.empty()?"Select crosshair":selected.c_str())){
        std::vector<std::filesystem::path> files;std::error_code ec;
        for(std::filesystem::directory_iterator i(directory,ec),end;!ec&&i!=end;i.increment(ec))
            if(i->is_regular_file(ec)&&i->path().extension()==".crosshair")files.push_back(i->path());
        std::sort(files.begin(),files.end());
        for(const auto& file:files)if(ImGui::Selectable(file.stem().string().c_str(),selected==file.stem().string())){
            if(loadCrosshairPreset(app,file)){selected=file.stem().string();app.status="Loaded crosshair "+selected;}else app.status="Invalid crosshair preset";
        }
        ImGui::EndCombo();
    }
    ImGui::InputText("Preset name",name.data(),name.size());
    if(ImGui::Button("Save crosshair")){
        std::string safe=name.data();for(auto& c:safe)if(!std::isalnum(static_cast<unsigned char>(c))&&c!='-'&&c!='_')c='_';
        if(safe.empty())safe="custom";
        if(saveCrosshairPreset(app,directory/(safe+".crosshair"))){selected=safe;app.status="Saved crosshair "+safe;}else app.status="Could not save crosshair";
    }
    ImGui::SameLine();
    if(ImGui::Button("Save as default##crosshair"))app.status=saveCrosshairPreset(app,directory/"default.crosshair")?"Saved default crosshair":"Could not save default crosshair";
    ImGui::SameLine();
    if(ImGui::Button("Load default##crosshair"))app.status=loadCrosshairPreset(app,directory/"default.crosshair")?"Loaded default crosshair":"No valid default crosshair";
}

