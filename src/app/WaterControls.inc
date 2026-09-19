void drawWaterControls(AppState& app){
    if(!ImGui::CollapsingHeader("Water level"))return;
    auto& w=app.water;ImGui::PushID("experimental_water");
    ImGui::Checkbox("Enable water",&w.enabled);
    uiHelp("Visual only. No collision or swimming. New maps disable water; placement is session-only.");
    ImGui::DragFloat("Height",&w.height,1.f,-1000000.f,1000000.f,"%.1f cm");
    if(ImGui::Button("At player feet"))w.height=app.actorPosition.z;
    ImGui::SameLine();if(ImGui::Button("At camera target"))w.height=app.cameraTarget.z;
    if(ImGui::BeginCombo("Surface look","Choose a look")){
        const char* names[]={"Ocean","Calm","Rain on still water","Rough sea","Stylized turquoise sea"};
        for(int n=0;n<5;++n)if(ImGui::Selectable(names[n]))static_cast<render::water::Appearance&>(w)=render::water::preset(n);
        ImGui::EndCombo();
    }
    uiHelp("Water appearance is included in Visual presets. Loading a visual preset never changes height or enables water.");
    if(ImGui::TreeNode("Waves")){
        ImGui::SliderFloat("Wave height",&w.waveHeight,0,12,"%.2f m");
        ImGui::SliderFloat("Wavelength",&w.wavelength,1,100,"%.1f m");
        ImGui::SliderFloat("Choppiness",&w.choppiness,0,8);
        ImGui::SliderFloat("Crest tightness",&w.surface.tightness,0,1);
        ImGui::SliderFloat("Wind direction",&w.direction,-180,180,"%.0f deg");
        ImGui::SliderFloat("Directional spread",&w.spread,0,1);
        ImGui::SliderFloat("Cross swell",&w.crossSwell,0,1);
        ImGui::SliderFloat("Swell crossing angle",&w.swellAngle,-180,180,"%.0f deg");
        ImGui::SliderFloat("Swell length multiplier",&w.swellLength,.5f,4,"%.2fx");
        ImGui::SliderInt("Pattern seed",&w.seed,0,10000);
        ImGui::SliderFloat("Small ripples",&w.detail,0,2);
        ImGui::SliderFloat("Crest foam",&w.foam,0,2);
        ImGui::SliderFloat("Foam coverage",&w.optics.foamCoverage,0,1);
        ImGui::SliderFloat("Foam pattern scale",&w.optics.foamScale,.1f,10,"%.2f m");
        ImGui::SliderFloat("Foam trail duration",&w.surface.foamPersistence,0,4,"%.2f sec");
        ImGui::SliderFloat("Foam bubble detail",&w.surface.foamDetail,0,1);
        ImGui::TreePop();
    }
    if(ImGui::TreeNode("Rain ripples")){
        ImGui::SliderFloat("Rain amount",&w.rain,0,2);
        ImGui::SliderFloat("Ripple scale",&w.rainScale,.1f,3,"%.2f m");
        ImGui::SliderFloat("Rain speed",&w.rainSpeed,.1f,4,"%.2fx");
        ImGui::TreePop();
    }
    if(ImGui::TreeNode("Material")){
        ImGui::ColorEdit3("Deep water color",&w.color.x);
        ImGui::SliderFloat("Roughness",&w.roughness,.03f,1);
        ImGui::SliderFloat("Reflection",&w.reflection,0,3);
        ImGui::TreePop();
    }
    if(ImGui::TreeNode("Depth & crest lighting")){
        ImGui::Checkbox("Depth shading",&w.optics.depthEnabled);
        ImGui::ColorEdit3("Shallow water color",&w.optics.shallow.x);
        ImGui::SliderFloat("Absorption distance",&w.optics.absorption,.1f,100,"%.2f m",ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Underwater visibility",&w.optics.transmission,0,1);
        ImGui::SliderFloat("Crest translucency",&w.optics.crestLight,0,3);
        ImGui::SliderFloat("Shoreline foam",&w.optics.shoreFoam,0,2);
        ImGui::SliderFloat("Shoreline width",&w.optics.shoreWidth,.01f,5,"%.2f m");
        ImGui::TreePop();
    }
    if(ImGui::TreeNode("Timing & quality")){
        ImGui::Checkbox("Animate",&w.animate);
        ImGui::SliderFloat("Speed",&w.speed,0,4,"%.2fx");
        ImGui::DragFloat("Time offset",&w.timeOffset,.05f,-86400,86400,"%.2f sec");
        uiHelp("Replay and capture use replay time. Disable Animate to freeze at Time offset. Water is not written into takes.");
        ImGui::SliderFloat("Coverage half-width",&w.radius,25,2000,"%.0f m",ImGuiSliderFlags_Logarithmic);
        ImGui::Combo("Geometry quality",&w.quality,"Low\0Medium\0High\0");
        uiHelp("A fixed-size projected grid concentrates detail in view. Unresolved crest tips are filtered; higher quality improves silhouettes, not foam detail.");
        ImGui::TreePop();
    }
    w.sanitize();
    if(!app.renderer.waterError().empty())ImGui::TextWrapped("Water shader: %s",app.renderer.waterError().c_str());
    ImGui::PopID();
}
