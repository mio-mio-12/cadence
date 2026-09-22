void drawHitmarker(const AppState& app,ImDrawList* draw,ImVec2 center,int kind,float remaining){
    if(!app.hitmarkerEnabled||remaining<=0)return;
    const float duration=kind==3?.24f:kind==2?.20f:.13f;
    const float phase=std::clamp(remaining/duration,0.f,1.f);
    const float fade=std::pow(phase,std::max(.01f,app.hitmarkerFadePower))*std::clamp(app.hitmarkerOpacity,0.f,1.f);
    const float scale=std::clamp((kind==3?app.hitmarkerKillScale:1.f)*(1.f+app.hitmarkerPulse*phase),0.f,100.f);
    auto color=kind==3?app.hitmarkerKillColor:kind==2?app.hitmarkerHeadColor:app.hitmarkerColor;
    color.w*=fade;
    const auto ink=ImGui::ColorConvertFloat4ToU32(color);
    const auto sprite=kind==3&&app.killmarkerTextureId?app.killmarkerTextureId:app.hitmarkerTextureId;
    if(sprite&&!app.hitmarkerProcedural){
        const float radius=std::clamp(app.hitmarkerSpriteSize*scale*.5f,0.f,4096.f);
        draw->AddImage(static_cast<ImTextureID>(sprite),{center.x-radius,center.y-radius},{center.x+radius,center.y+radius},{0,0},{1,1},ink);
        return;
    }
    const float gap=std::clamp(app.hitmarkerGap*scale,0.f,4096.f),length=std::clamp(app.hitmarkerLength*scale,0.f,4096.f);
    const float thickness=std::clamp(app.hitmarkerThickness,.1f,256.f),outline=std::clamp(app.hitmarkerOutline,0.f,128.f);
    auto edge=app.hitmarkerOutlineColor;edge.w*=fade;const auto edgeInk=ImGui::ColorConvertFloat4ToU32(edge);
    const int arms=std::clamp(app.hitmarkerArms,1,32);
    // Outline all arms before their foreground so intersections remain clean.
    for(int pass=outline>0?0:1;pass<2;++pass){
        for(int i=0;i<arms;++i){
            const float angle=(app.hitmarkerRotation+i*360.f/arms)*scene::kPi/180.f;
            const float x=std::cos(angle),y=std::sin(angle);
            draw->AddLine({center.x+x*gap,center.y+y*gap},{center.x+x*(gap+length),center.y+y*(gap+length)},pass?ink:edgeInk,thickness+(pass?0:2*outline));
        }
        if(app.hitmarkerDot)draw->AddCircleFilled(center,std::clamp(app.hitmarkerDotSize*scale,0.f,256.f)+(pass?0:outline),pass?ink:edgeInk);
    }
}

void drawHitmarkerControls(AppState& app){
    if(!ImGui::CollapsingHeader("Hitmarker"))return;
    ImGui::PushID("hitmarker");
    ImGui::Checkbox("Enabled",&app.hitmarkerEnabled);
    ImGui::Checkbox("Use procedural shape",&app.hitmarkerProcedural);
    if(ImGui::TreeNode("Shape")){
        ImGui::SliderInt("Arms",&app.hitmarkerArms,1,16);
        ImGui::SliderFloat("Length",&app.hitmarkerLength,0,60);
        ImGui::SliderFloat("Gap",&app.hitmarkerGap,0,40);
        ImGui::SliderFloat("Thickness",&app.hitmarkerThickness,.1f,12);
        ImGui::SliderFloat("Rotation",&app.hitmarkerRotation,-180,180,"%.1f deg");
        ImGui::SliderFloat("Outline",&app.hitmarkerOutline,0,8);
        ImGui::Checkbox("Center dot",&app.hitmarkerDot);
        ImGui::SliderFloat("Dot radius",&app.hitmarkerDotSize,0,12);
        ImGui::SliderFloat("Kill scale",&app.hitmarkerKillScale,0,3,"%.2fx");
        ImGui::TreePop();
    }
    if(ImGui::TreeNode("Color & timing")){
        ImGui::ColorEdit4("Hit",&app.hitmarkerColor.x);
        ImGui::ColorEdit4("Headshot",&app.hitmarkerHeadColor.x);
        ImGui::ColorEdit4("Kill",&app.hitmarkerKillColor.x);
        ImGui::ColorEdit4("Outline color",&app.hitmarkerOutlineColor.x);
        ImGui::SliderFloat("Opacity",&app.hitmarkerOpacity,0,1);
        ImGui::SliderFloat("Duration",&app.hitmarkerDurationScale,.1f,10,"%.2fx");
        ImGui::SliderFloat("Fade curve",&app.hitmarkerFadePower,.05f,8);
        ImGui::SliderFloat("Expansion pulse",&app.hitmarkerPulse,0,3);
        ImGui::TreePop();
    }
    if(ImGui::TreeNode("Images")){
        for(int i=0;i<2;++i){
            ImGui::PushID(i);
            auto& path=i?app.killmarkerTexturePath:app.hitmarkerTexturePath;
            auto& id=i?app.killmarkerTextureId:app.hitmarkerTextureId;
            if(ImGui::Button(i?"Load kill image":"Load hit image"))if(const auto selected=chooseImageFile()){
                const auto texture=app.renderer.loadTexture(*selected,false,false);
                if(texture){path=*selected;id=texture;app.hitmarkerProcedural=false;}else app.status="Could not load hitmarker image";
            }
            ImGui::SameLine();if(ImGui::Button("Clear")){path.clear();id=0;}
            ImGui::PopID();
        }
        ImGui::SliderFloat("Image size",&app.hitmarkerSpriteSize,1,160);
        ImGui::TreePop();
    }
    static int previewKind=1;static float previewTime{};
    const char* labels[]{"Preview hit","Preview headshot","Preview kill"};
    for(int i=0;i<3;++i){if(i)ImGui::SameLine();if(ImGui::Button(labels[i])){previewKind=i+1;previewTime=i==2?.24f:i==1?.20f:.13f;}}
    const ImVec2 top=ImGui::GetCursorScreenPos(),size{ImGui::GetContentRegionAvail().x,100};
    ImGui::InvisibleButton("Preview",size);
    auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled(top,{top.x+size.x,top.y+size.y},IM_COL32(35,38,43,255));
    draw->PushClipRect(top,{top.x+size.x,top.y+size.y},true);
    drawHitmarker(app,draw,{top.x+size.x*.5f,top.y+size.y*.5f},previewKind,previewTime>0?previewTime:(previewKind==3?.24f:previewKind==2?.20f:.13f));
    draw->PopClipRect();previewTime=std::max(0.f,previewTime-ImGui::GetIO().DeltaTime/std::max(.01f,app.hitmarkerDurationScale));
    ImGui::PopID();
}
