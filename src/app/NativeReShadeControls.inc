void drawNativeReShadeControls(AppState& app){
    if(!ImGui::CollapsingHeader("ReShade"))return;
    auto& settings=app.nativeReShade;auto& runtime=app.nativeReShadeRuntime;
    if(ImGui::Checkbox("Enable ReShade",&settings.enabled)&&!settings.enabled)runtime.shutdown();
    if(!settings.enabled)return;
    const auto root=programDirectory();const auto folder=root/"ReShade"/"presets";std::error_code ec;
    if(ImGui::BeginCombo("ReShade preset",settings.preset.c_str())){
        if(std::filesystem::is_directory(folder,ec))for(const auto& file:std::filesystem::directory_iterator(folder,ec))if(lowerText(file.path().extension().string())==".ini")if(ImGui::Selectable(file.path().filename().string().c_str(),settings.preset==file.path().filename().string()))settings.preset=file.path().filename().string();
        ImGui::EndCombo();
    }
    ImGui::InputText("ReShade preset name",app.nativeReShadePresetName.data(),app.nativeReShadePresetName.size());
    ImGui::BeginDisabled(!runtime.ready());
    if(ImGui::Button("Save ReShade preset"))runtime.savePreset();ImGui::SameLine();
    if(ImGui::Button("Save ReShade preset as")){auto name=safePackageId(app.nativeReShadePresetName.data());if(!name.empty()){const auto path=folder/(name+".ini");runtime.exportPreset(path);settings.preset=path.filename().string();}}
    ImGui::EndDisabled();
    if(ImGui::Button("Reload shaders"))runtime.reload();ImGui::SameLine();
    if(ImGui::Button("Open shader folder")){
#ifdef _WIN32
        const auto path=root/"reshade-shaders";std::filesystem::create_directories(path,ec);ShellExecuteW(nullptr,L"open",path.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
#endif
    }
    if(!runtime.error().empty())ImGui::TextWrapped("%s",runtime.error().c_str());
    if(ImGui::Button("Open ReShade log")){
#ifdef _WIN32
        const auto log=root/"ReShade"/"ReShade.log";ShellExecuteW(nullptr,L"open",log.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
#endif
    }
    static ImGuiTextFilter search;search.Draw("Search effects");
    const auto techniques=runtime.techniques();if(techniques.empty())ImGui::TextDisabled("No techniques loaded");
    for(const auto& technique:techniques){if(!search.PassFilter(technique.name.c_str())&&!search.PassFilter(technique.effect.c_str()))continue;
        ImGui::PushID(technique.effect.c_str());ImGui::PushID(technique.name.c_str());
        bool enabled=technique.enabled;if(ImGui::Checkbox(technique.name.c_str(),&enabled))runtime.enableTechnique(technique.handle,enabled);ImGui::SameLine();
        if(ImGui::TreeNode("Settings")){
            ImGui::TextDisabled("%s",technique.effect.c_str());
            for(auto uniform:runtime.uniforms(technique.effect)){
                ImGui::PushID(uniform.name.c_str());bool changed=false;
                if(uniform.type==3&&uniform.count==1){bool v=uniform.value[0]!=0;changed=ImGui::Checkbox(uniform.label.c_str(),&v);uniform.value[0]=v?1.f:0.f;}
                else if(uniform.type!=0){int v[4]{};for(int i=0;i<uniform.count;++i)v[i]=int(uniform.value[i]);changed=ImGui::DragScalarN(uniform.label.c_str(),ImGuiDataType_S32,v,uniform.count,1);for(int i=0;i<uniform.count;++i)uniform.value[i]=float(v[i]);}
                else if(uniform.ui=="color"&&uniform.count==3)changed=ImGui::ColorEdit3(uniform.label.c_str(),uniform.value.data());
                else if(uniform.ui=="color"&&uniform.count==4)changed=ImGui::ColorEdit4(uniform.label.c_str(),uniform.value.data());
                else if(uniform.ranged)changed=ImGui::SliderScalarN(uniform.label.c_str(),ImGuiDataType_Float,uniform.value.data(),uniform.count,&uniform.minimum,&uniform.maximum,"%.3f");
                else changed=ImGui::DragScalarN(uniform.label.c_str(),ImGuiDataType_Float,uniform.value.data(),uniform.count,.01f,nullptr,nullptr,"%.3f");
                if(changed)runtime.setUniform(uniform);ImGui::PopID();
            }
            ImGui::TreePop();
        }
        ImGui::PopID();ImGui::PopID();
    }
}
