#include "app/ScoreFontPicker.inc"
void drawScorePresetPicker(AppState& app){
    auto folder=cadenceAssetsDirectory()/"score_presets";std::error_code ec;
    if(ImGui::BeginCombo("Score preset",app.scorePresetName.data())){
        if(std::filesystem::is_directory(folder,ec))for(const auto& f:std::filesystem::directory_iterator(folder,ec))if(f.path().extension()==".score")
            if(ImGui::Selectable(f.path().stem().string().c_str())){if(gameplay::score::load(app.scoreSettings,f.path())){std::snprintf(app.scorePresetName.data(),app.scorePresetName.size(),"%s",f.path().stem().string().c_str());app.scoreState={};app.scorePreview.restart();if(std::none_of(app.interfaceFonts.begin(),app.interfaceFonts.end(),[&](const auto& font){return font.first==app.scoreSettings.font;}))app.pendingScoreFont=std::filesystem::path(app.scoreSettings.font).filename().string();}else app.status="Invalid score preset";}
        ImGui::EndCombo();
    }
}
void drawScoreImage(AppState& app,ImVec2 origin,ImVec2 size,const gameplay::score::State& state,double time,bool guides=false){
    const auto& settings=app.scoreSettings;const auto p=gameplay::score::sample(settings,state,time);
    auto* d=ImGui::GetWindowDrawList();const float scale=std::max(.001f,size.y/720.f);
    const auto anchor=gameplay::score::anchor(settings,size.x,size.y);
    const gameplay::score::Point location{origin.x+anchor.x+p.x*scale,origin.y+anchor.y+p.y*scale};
    d->PushClipRect(origin,{origin.x+size.x,origin.y+size.y},true);
    if(p.alpha>0){
        auto* font=ImGui::GetFont();for(const auto& item:app.interfaceFonts)if(item.first==settings.font)font=item.second;
        const float pixels=24.f*scale;const auto measured=font->CalcTextSizeA(pixels,FLT_MAX,0,p.text.c_str());
        // Generate glyphs without pre-transform CPU culling. The draw command's
        // canvas scissor still clips the final rotated/scaled geometry on GPU.
        const auto text=[&](ImVec2 pos,ImU32 color){font->RenderText(d,pixels,pos,color,{-1000000,-1000000,1000000,1000000},p.text.c_str(),nullptr);};
        const int first=d->VtxBuffer.Size;
        static gameplay::score::EffectKernelCache shadowKernel,glowKernel;
        const auto effect=[&](const float* color,float opacity,float direction,float offset,float softness,gameplay::score::EffectKernelCache& kernel){
            const float a=direction*scene::kPi/180,dx=std::cos(a)*offset,dy=std::sin(a)*offset;
            for(const auto& tap:kernel.get(softness)){const auto packed=ImGui::ColorConvertFloat4ToU32({color[0],color[1],color[2],p.alpha*opacity*tap.weight});if((packed&IM_COL32_A_MASK)==0)continue;text({location.x+(dx+tap.x)*scale,location.y+(dy+tap.y)*scale},packed);}
        };
        if(settings.shadow)effect(settings.shadowColor,settings.shadowAlpha,settings.shadowDirection,settings.shadowOffset,settings.shadowSoftness,shadowKernel);
        if(settings.glow)effect(settings.glowColor,settings.glowAlpha,settings.glowDirection,settings.glowOffset,settings.glowSoftness,glowKernel);
        const int textFirst=d->VtxBuffer.Size;
        text({location.x,location.y},ImGui::ColorConvertFloat4ToU32({settings.color[0],settings.color[1],settings.color[2],p.alpha}));
        gameplay::score::Point centroid{measured.x*.5f,measured.y*.5f};
        if(d->VtxBuffer.Size>textFirst){centroid={};for(int i=textFirst;i<d->VtxBuffer.Size;++i){centroid.x+=d->VtxBuffer[i].pos.x-location.x;centroid.y+=d->VtxBuffer[i].pos.y-location.y;}const auto count=float(d->VtxBuffer.Size-textFirst);centroid.x/=count;centroid.y/=count;}
        const auto pivot=gameplay::score::pivot(settings,{measured.x,measured.y},centroid);
        for(int i=first;i<d->VtxBuffer.Size;++i){auto& v=d->VtxBuffer[i];const auto point=gameplay::score::transform({v.pos.x-location.x,v.pos.y-location.y},location,pivot,p.scale,p.rotation);v.pos={point.x,point.y};}
    }
    if(guides){
        const ImVec2 a{origin.x+anchor.x,origin.y+anchor.y},b{location.x,location.y};
        const auto col=ImGui::GetColorU32(ImGuiCol_TextDisabled);d->AddLine(a,b,col);
        d->AddLine({a.x-5,a.y},{a.x+5,a.y},col);d->AddLine({a.x,a.y-5},{a.x,a.y+5},col);
        d->AddCircle(b,4,ImGui::GetColorU32(ImGuiCol_Text));
    }
    d->PopClipRect();
}
void drawScoreWorkspace(AppState& app){
    auto& s=app.scoreSettings;auto& preview=app.scorePreview;preview.advance(s,ImGui::GetTime());
    if(!ImGui::BeginTable("Score workspace",2,ImGuiTableFlags_Resizable|ImGuiTableFlags_BordersInnerV))return;
    ImGui::TableSetupColumn("Settings",ImGuiTableColumnFlags_WidthStretch,.45f);ImGui::TableSetupColumn("Preview",ImGuiTableColumnFlags_WidthStretch,.55f);
    ImGui::TableNextColumn();ImGui::BeginChild("Score settings",{0,0},false);
    drawScorePresetPicker(app);ImGui::InputText("Preset name",app.scorePresetName.data(),app.scorePresetName.size());
    if(ImGui::Button("Save preset")){auto name=safePackageId(app.scorePresetName.data());app.status=gameplay::score::save(s,cadenceAssetsDirectory()/"score_presets"/(name+".score"))?"Score preset saved":"Score preset save failed";}
    ImGui::SameLine();if(ImGui::Button("Reset settings")){s={};preview.restart();}
    drawScoreFontPicker(app);
    ImGui::DragInt("Score per kill",&s.perKill,1,-1000000,1000000);
    ImGui::Checkbox("Stack scores",&s.stacking);ImGui::DragFloat("Stack window",&s.window,.01f,0,30,"%.2f s",ImGuiSliderFlags_AlwaysClamp);
    if(ImGui::CollapsingHeader("Transform",ImGuiTreeNodeFlags_DefaultOpen)){
        const char* anchors[]={"Screen center","Top left","Top right","Bottom left","Bottom right","Custom"};
        const char* pivots[]={"Text center","Top left","Bottom center","Custom","Center of points"};
        ImGui::Combo("Position anchor",&s.anchor,anchors,6);if(s.anchor==5){ImGui::SliderFloat("Anchor X",&s.anchorX,0,1,"%.3f");ImGui::SliderFloat("Anchor Y",&s.anchorY,0,1,"%.3f");}
        ImGui::DragFloat2("Position",&s.x,1,-2000,2000);
        ImGui::Combo("Transform origin",&s.pivot,pivots,5);if(s.pivot==3){ImGui::DragFloat("Origin X",&s.pivotX,.01f,-2,3,"%.3f");ImGui::DragFloat("Origin Y",&s.pivotY,.01f,-2,3,"%.3f");}
        ImGui::DragFloat("Rotation",&s.rotation,.5f,-360,360,"%.1f deg");
        if(ImGui::Button("Center position")){s.anchor=0;s.x=s.y=0;}
        ImGui::SameLine();if(ImGui::Button("Reset transform")){s.anchor=s.pivot=0;s.anchorX=s.anchorY=s.pivotX=s.pivotY=.5f;s.x=s.y=s.rotation=s.enterRotation=s.exitRotation=0;}
    }
    const auto time=[](const char* label,float& v){ImGui::DragFloat(label,&v,.01f,0,30,"%.2f s",ImGuiSliderFlags_AlwaysClamp);};
    if(ImGui::CollapsingHeader("Animation",ImGuiTreeNodeFlags_DefaultOpen)){
        ImGui::DragFloat2("Enter offset",&s.enterX,1,-2000,2000);ImGui::DragFloat2("Exit offset",&s.exitX,1,-2000,2000);
        ImGui::DragFloat("Enter rotation",&s.enterRotation,.5f,-360,360,"%.1f deg");ImGui::DragFloat("Exit rotation",&s.exitRotation,.5f,-360,360,"%.1f deg");
        time("Move in",s.moveIn);time("Impact in",s.impactIn);time("Impact out",s.impactOut);time("Hold",s.hold);time("Fade",s.fade);
        ImGui::DragFloat("Start scale",&s.startScale,.01f,.01f,10,"%.2f",ImGuiSliderFlags_AlwaysClamp);ImGui::DragFloat("Impact scale",&s.overshootScale,.01f,.01f,10,"%.2f",ImGuiSliderFlags_AlwaysClamp);ImGui::DragFloat("Final scale",&s.finalScale,.01f,.01f,10,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderInt("Scramble frames",&s.shuffleFrames,0,20);time("Scramble step",s.shuffleStep);
    }
    if(ImGui::CollapsingHeader("Appearance",ImGuiTreeNodeFlags_DefaultOpen)){
        ImGui::ColorEdit3("Color",s.color);
        const auto layer=[](const char* name,bool& enabled,float* color,float& opacity,float& direction,float& offset,float& softness){
            ImGui::PushID(name);ImGui::Checkbox(name,&enabled);
            if(enabled){ImGui::SliderFloat("Direction",&direction,0,360,"%.1f deg",ImGuiSliderFlags_AlwaysClamp);ImGui::DragFloat("Offset",&offset,.1f,0,1000,"%.1f px",ImGuiSliderFlags_AlwaysClamp);ImGui::DragFloat("Softness",&softness,.1f,0,64,"%.1f px",ImGuiSliderFlags_AlwaysClamp);ImGui::SliderFloat("Opacity",&opacity,0,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);ImGui::ColorEdit3("Color",color);}
            ImGui::PopID();
        };
        layer("Drop shadow",s.shadow,s.shadowColor,s.shadowAlpha,s.shadowDirection,s.shadowOffset,s.shadowSoftness);
        layer("Glow",s.glow,s.glowColor,s.glowAlpha,s.glowDirection,s.glowOffset,s.glowSoftness);
    }
    ImGui::EndChild();ImGui::TableNextColumn();ImGui::BeginChild("Score preview controls",{0,0},false);
    if(ImGui::Button(preview.playing?"Pause preview":"Play preview"))preview.playing=!preview.playing;ImGui::SameLine();if(ImGui::Button("Restart"))preview.restart();ImGui::SameLine();ImGui::Checkbox("Loop",&preview.loop);
    if(ImGui::SliderFloat("Preview time",&preview.time,0,gameplay::score::duration(s),"%.2f s"))preview.playing=false;
    ImGui::SliderFloat("Playback speed",&preview.speed,.1f,3,"%.2fx");if(preview.loop)time("Loop gap",preview.gap);
    ImGui::Checkbox("Use score per kill",&preview.useKillAmount);if(!preview.useKillAmount)ImGui::DragInt("Preview amount",&preview.amount,1,-1000000,1000000);
    ImGui::Checkbox("Preview over POV",&preview.overPov);ImGui::SameLine();if(ImGui::Button("Open gameplay")){preview.overPov=true;app.workspaceMode=0;}
    ImGui::Checkbox("POV background",&preview.povBackground);ImGui::SameLine();ImGui::Checkbox("Show origins",&preview.guides);
    const auto space=ImGui::GetContentRegionAvail();
    const float aspect=app.lastViewportImageSize.y>0?app.lastViewportImageSize.x/app.lastViewportImageSize.y:16.f/9;
    const float width=std::max(1.f,std::min(space.x,space.y*aspect));const ImVec2 size{width,width/aspect};
    const auto origin=ImGui::GetCursorScreenPos();ImGui::InvisibleButton("Score canvas",size);
    if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(ImGuiMouseButton_Left)){const auto delta=ImGui::GetIO().MouseDelta;const float scale=std::max(.001f,size.y/720.f);s.x+=delta.x/scale;s.y+=delta.y/scale;}
    auto* d=ImGui::GetWindowDrawList();
    if(preview.povBackground&&app.renderer.colorTexture())d->AddImage(static_cast<ImTextureID>(app.renderer.colorTexture()),origin,{origin.x+size.x,origin.y+size.y},{0,1},{1,0});
    else d->AddRectFilled(origin,{origin.x+size.x,origin.y+size.y},ImGui::GetColorU32(ImGuiCol_FrameBg));
    drawScoreImage(app,origin,size,preview.state(s),preview.time,preview.guides);
    ImGui::EndChild();ImGui::EndTable();
}
