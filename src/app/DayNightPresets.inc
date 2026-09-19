bool saveDayNightPreset(const AppState&app,const std::filesystem::path&path){
    std::error_code error;std::filesystem::create_directories(path.parent_path(),error);if(error)return false;
    std::ofstream out(path);out.imbue(std::locale::classic());out<<std::setprecision(9);render::daynight::writePreset(out,{app.dayNight,app.nightSky});out.flush();return bool(out);
}
void applyDayNightPreset(AppState&app,const render::daynight::Preset&p){app.dayNight=p.cycle;app.nightSky=p.sky;app.dayNightSeconds=0;}
bool loadDayNightPreset(AppState&app,const std::filesystem::path&path){std::ifstream in(path);in.imbue(std::locale::classic());render::daynight::Preset p;if(!render::daynight::readPreset(in,p))return false;applyDayNightPreset(app,p);return true;}
void drawDayNightPresets(AppState&app){
    ImGui::PushID("day_night_presets");
    static std::string selected;static std::filesystem::path selectedFile;
    static std::array<char,96> name{"custom"};static std::vector<std::filesystem::path> files;
    const auto folder=cadenceAssetsDirectory()/"day_night_presets";
    if(ImGui::BeginCombo("Preset",selected.empty()?"Choose a day / night look":selected.c_str())){
        if(ImGui::IsWindowAppearing()){files.clear();std::error_code error;for(std::filesystem::directory_iterator i(folder,error),end;!error&&i!=end;i.increment(error))if(i->is_regular_file(error)&&i->path().extension()==".castdaynight")files.push_back(i->path());std::sort(files.begin(),files.end());}
        for(size_t i=0;i<render::daynight::presetNames.size();++i){const auto label=render::daynight::presetNames[i];if(ImGui::Selectable(label.data(),selected==label)){applyDayNightPreset(app,render::daynight::builtinPreset(i));selected=label;selectedFile.clear();app.status="Loaded day/night preset: "+selected;}}
        if(!files.empty())ImGui::Separator();
        for(const auto&file:files){ImGui::PushID(file.string().c_str());if(ImGui::Selectable(file.stem().string().c_str(),selectedFile==file)){if(loadDayNightPreset(app,file)){selected=file.stem().string();selectedFile=file;app.status="Loaded day/night preset: "+selected;}else app.status="Invalid day/night preset; settings unchanged";}ImGui::PopID();}
        ImGui::EndCombo();
    }
    if(ImGui::TreeNode("Manage presets")){
        ImGui::InputText("Preset name",name.data(),name.size());
        if(ImGui::Button("Save preset")){std::string safe=name.data();for(auto&c:safe)if(!std::isalnum(static_cast<unsigned char>(c))&&c!='-'&&c!='_')c='_';if(safe.empty())safe="custom";const auto file=folder/(safe+".castdaynight");if(saveDayNightPreset(app,file)){selected=safe;selectedFile=file;app.status="Saved day/night preset: "+safe;}else app.status="Could not save day/night preset";}
        ImGui::SameLine();if(ImGui::Button("Save as default"))app.status=saveDayNightPreset(app,folder/"default.castdaynight")?"Saved default day/night look":"Could not save default day/night look";
        ImGui::SameLine();ImGui::BeginDisabled(selectedFile.empty());if(ImGui::Button("Delete"))ImGui::OpenPopup("Delete saved preset?");ImGui::EndDisabled();
        if(ImGui::BeginPopupModal("Delete saved preset?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){ImGui::TextUnformatted(selected.c_str());if(ImGui::Button("Delete permanently")){std::error_code error;const bool removed=std::filesystem::remove(selectedFile,error);app.status=removed&&!error?"Deleted day/night preset":"Could not delete preset";if(removed){selected.clear();selectedFile.clear();}ImGui::CloseCurrentPopup();}ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
        ImGui::TreePop();
    }
    ImGui::PopID();
}
