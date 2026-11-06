void processScoreFonts(AppState& app){
    auto& browser=app.scoreFontBrowser;auto& atlas=*ImGui::GetIO().Fonts;
    if(!app.pendingScoreFont.empty()){
        const auto name=app.pendingScoreFont;const auto found=std::find_if(app.interfaceFonts.begin(),app.interfaceFonts.end(),[&](const auto& f){return f.first==name;});
        if(found!=app.interfaceFonts.end())app.scoreSettings.font=name;
        else{
            ImFont* font=nullptr;
            if(browser.previewName==name&&browser.preview){font=browser.preview;browser.preview=nullptr;browser.previewName.clear();}
            else {const auto path=std::filesystem::path("C:/Windows/Fonts")/std::filesystem::path(name).filename();if(std::filesystem::is_regular_file(path))font=atlas.AddFontFromFileTTF(path.string().c_str(),24.f);}
            if(font){app.interfaceFonts.push_back({name,font});app.scoreSettings.font=name;}
        }
        app.pendingScoreFont.clear();
    }
    if(!browser.pendingPreview.empty()){
        const auto name=browser.pendingPreview;browser.pendingPreview.clear();
        if(browser.preview)atlas.RemoveFont(browser.preview);browser.preview=nullptr;browser.previewName=name;
        const auto found=std::find_if(app.interfaceFonts.begin(),app.interfaceFonts.end(),[&](const auto& f){return f.first==name;});
        if(found==app.interfaceFonts.end()){const auto path=std::filesystem::path("C:/Windows/Fonts")/std::filesystem::path(name).filename();if(std::filesystem::is_regular_file(path))browser.preview=atlas.AddFontFromFileTTF(path.string().c_str(),24.f);}
    }
}
void drawScoreFontPicker(AppState& app){
    auto& browser=app.scoreFontBrowser;auto& selected=app.scoreSettings.font;
    static std::vector<std::string> windowsFonts;
    if(ImGui::BeginCombo("Font",selected.c_str())){
        if(windowsFonts.empty()){std::error_code ec;for(const auto& f:std::filesystem::directory_iterator("C:/Windows/Fonts",ec)){const auto ext=lowerText(f.path().extension().string());if(ext==".ttf"||ext==".otf"||ext==".ttc")windowsFonts.push_back(f.path().filename().string());}std::sort(windowsFonts.begin(),windowsFonts.end());}
        if(ImGui::IsWindowAppearing())ImGui::SetKeyboardFocusHere();ImGui::InputTextWithHint("##font_search","Search fonts",browser.search.data(),browser.search.size());
        struct Row{std::string name;ImFont* font{};};std::vector<Row> rows;
        for(const auto& f:app.interfaceFonts)if(cadence::fonts::matches(f.first,browser.search.data()))rows.push_back({f.first,f.second});
        for(const auto& name:windowsFonts)if(cadence::fonts::matches(name,browser.search.data())&&std::none_of(app.interfaceFonts.begin(),app.interfaceFonts.end(),[&](const auto& f){return f.first==name;}))rows.push_back({name,browser.previewName==name?browser.preview:nullptr});
        if(ImGui::BeginTable("Font choices",2,ImGuiTableFlags_SizingStretchProp)){
            ImGui::TableSetupColumn("Name",ImGuiTableColumnFlags_WidthStretch);ImGui::TableSetupColumn("Preview",ImGuiTableColumnFlags_WidthFixed,64);
            ImGuiListClipper clipper;clipper.Begin(int(rows.size()),ImGui::GetTextLineHeightWithSpacing());
            while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i){const auto& row=rows[i];ImGui::PushID(row.name.c_str());ImGui::TableNextRow();ImGui::TableNextColumn();
                if(ImGui::Selectable(row.name.c_str(),selected==row.name)){if(row.font&&row.font!=browser.preview)selected=row.name;else app.pendingScoreFont=row.name;ImGui::CloseCurrentPopup();}
                if(!row.font&&(ImGui::IsItemHovered()||ImGui::IsItemFocused()))browser.hover(row.name,ImGui::GetTime());
                ImGui::TableNextColumn();if(row.font){const auto size=ImGui::GetFontSize();ImGui::PushFont(row.font,size);ImGui::TextUnformatted("+100");ImGui::PopFont();}ImGui::PopID();
            }
            ImGui::EndTable();
        }
        if(rows.empty())ImGui::TextDisabled("No matching fonts");ImGui::EndCombo();
    }
    ImFont* font=nullptr;for(const auto& f:app.interfaceFonts)if(f.first==selected)font=f.second;
    if(font){ImGui::SameLine();const auto size=ImGui::GetFontSize();ImGui::PushFont(font,size);ImGui::TextUnformatted("+100");ImGui::PopFont();}
}
