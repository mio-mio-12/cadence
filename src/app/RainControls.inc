void drawRainControls(AppState& app){
    if(!ImGui::CollapsingHeader("Rain"))return;
    ImGui::PushID("rain");auto& s=app.rain;
    ImGui::Checkbox("Enable rain",&s.enabled);
    if(ImGui::BeginCombo("Preset","Choose a look")){
        const char* names[]={"Drizzle","Steady rain","Downpour","Night rain","Wind-driven squall"};
        for(int n=0;n<5;++n)if(ImGui::Selectable(names[n]))s=render::rain::preset(n);
        ImGui::EndCombo();
    }
    ImGui::SliderFloat("Intensity",&s.density,0,1,"%.2f");
    if(ImGui::TreeNode("Drops")){
        ImGui::SliderFloat("Fall speed",&s.speed,2,20,"%.1f m/s");
        ImGui::SliderFloat("Width",&s.width,.01f,.5f,"%.3f cm");
        ImGui::SliderFloat("Streak exposure",&s.shutter,.002f,.1f,"%.3f sec");
        ImGui::SliderFloat("Opacity",&s.opacity,0,1,"%.2f");
        ImGui::SliderFloat("Brightness",&s.brightness,0,8,"%.2fx");
        ImGui::ColorEdit3("Tint",&s.tint.x,ImGuiColorEditFlags_HDR|ImGuiColorEditFlags_Float);
        ImGui::Checkbox("Refraction",&s.style.refractive);
        ImGui::SliderFloat("Color / refraction",&s.style.refractionMix,0,1);
        ImGui::SliderFloat("IOR",&s.style.ior,1,2);
        ImGui::SliderFloat("Refraction roughness",&s.style.roughness,0,1);
        ImGui::SliderFloat("Refraction strength",&s.style.refractionStrength,0,20);
        ImGui::TreePop();
    }
    if(ImGui::TreeNodeEx("Wind & turbulence",ImGuiTreeNodeFlags_DefaultOpen)){
        ImGui::SliderFloat("Wind speed",&s.wind,0,12,"%.1f m/s");
        ImGui::SliderFloat("Direction",&s.direction,-180,180,"%.0f deg");
        ImGui::SliderFloat("Gust strength",&s.gusts,0,2,"%.2fx");
        ImGui::SliderFloat("Gust period",&s.gustPeriod,.5f,20,"%.1f sec");
        ImGui::SliderFloat("Turbulence",&s.turbulence,0,3,"%.2fx");
        ImGui::SliderFloat("Swirl size",&s.turbulenceScale,.3f,15,"%.1f m");
        ImGui::SliderFloat("Swirl speed",&s.turbulenceRate,0,4,"%.2fx");
        ImGui::TreePop();
    }
    if(ImGui::TreeNode("Surfaces")){
        ImGui::Checkbox("Shelter collision",&s.shelter);
        ImGui::SliderFloat("Maximum blocker distance",&s.blockerDistance,0,200,"%.1f m");
        ImGui::Checkbox("Splashes",&s.splashes);
        ImGui::SliderFloat("Splash amount",&s.splashAmount,0,1,"%.2f");
        ImGui::SliderFloat("Splash size",&s.splashSize,.5f,15,"%.1f cm");
        if(ImGui::TreeNode("Splash shape")){
            ImGui::SliderFloat("Rate",&s.style.rate,.1f,5);ImGui::SliderFloat("Lifetime",&s.style.lifetime,.02f,2,"%.2f sec");
            ImGui::SliderFloat("Position randomness",&s.style.jitter,0,2);ImGui::SliderFloat("Fade",&s.style.fade,.1f,6);
            ImGui::SliderFloat("Ring opacity",&s.style.ringOpacity,0,2);ImGui::SliderFloat("Ring width",&s.style.ringWidth,.001f,.3f);
            ImGui::SliderFloat("Expansion",&s.style.expansion,0,8);ImGui::SliderFloat("Crown height",&s.style.crownHeight,0,6);
            ImGui::SliderFloat("Crown spread",&s.style.crownSpread,0,4);ImGui::SliderFloat("Spray size",&s.style.dropSize,.1f,4);
            ImGui::SliderFloat("Spray count",&s.style.dropCount,1,32,"%.0f");ImGui::TreePop();
        }
        ImGui::SliderFloat("Soft intersections",&s.softness,1,100,"%.1f cm");
        ImGui::Checkbox("Wall impact mist",&s.wallMist);
        ImGui::SliderFloat("Mist amount",&s.mistAmount,0,2,"%.2fx");
        ImGui::SliderFloat("Mist size",&s.mistSize,5,150,"%.0f cm");
        ImGui::SliderFloat("Aerosol detail",&s.style.grainScale,4,256);
        ImGui::SliderFloat("Aerosol amount",&s.style.grainAmount,0,1);
        ImGui::SliderFloat("Aerosol contrast",&s.style.grainContrast,.1f,8);
        ImGui::TreePop();
    }
    if(ImGui::TreeNode("Timing & quality")){
        if(ImGui::Checkbox("Animate",&s.animate)&&!s.animate)s.freezeTime=float(app.takePreview?app.takeTime:app.rainSeconds);
        ImGui::SliderFloat("Speed",&s.timeScale,0,4,"%.2fx");
        ImGui::DragFloat("Time offset",&s.timeOffset,.01f,0,0,"%.3f sec");
        ImGui::InputInt("Seed",&s.seed);
        ImGui::Combo("Quality",&s.quality,"Low\0High\0Cinematic\0");
        ImGui::SliderFloat("Range",&s.radius,5,50,"%.1f m");
        ImGui::SliderFloat("Camera clearance",&s.nearFade,5,200,"%.0f cm");
        ImGui::TreePop();
    }
    s.sanitize();
    if(!app.renderer.rainError().empty())ImGui::TextWrapped("%s",app.renderer.rainError().c_str());
    ImGui::PopID();
}
void drawWetControls(AppState& app){
    if(!ImGui::CollapsingHeader("Wet surfaces"))return;
    ImGui::PushID("wet_surfaces");auto& s=app.wet;
    ImGui::Checkbox("Wet ground",&s.ground);
    if(ImGui::TreeNode("Puddles")){
        ImGui::SliderFloat("Coverage",&s.coverage,0,1);ImGui::SliderFloat("Patch size",&s.patchSize,10,1000,"%.0f cm");
        ImGui::SliderFloat("Edge softness",&s.edge,.002f,.3f);ImGui::SliderFloat("Ripples",&s.ripples,0,2);
        ImGui::SliderFloat("Edge noise",&s.detail.edgeNoise,0,1);ImGui::SliderFloat("Edge detail",&s.detail.edgeDetail,1,64);
        ImGui::SliderFloat("Puddle roughness",&s.detail.puddleRoughness,.025f,1);
        ImGui::SliderFloat("Puddle reflectance",&s.detail.reflectance,0,1);
        ImGui::SliderFloat("Cubemap reflectance",&s.detail.reflection,0,4);
        ImGui::TreePop();
    }
    ImGui::Checkbox("Surface rain",&s.enabled);
    if(ImGui::TreeNodeEx("Targets",ImGuiTreeNodeFlags_DefaultOpen)){
        ImGui::Checkbox("Map",&s.map);ImGui::SameLine();ImGui::Checkbox("Playermodels",&s.actors);ImGui::SameLine();ImGui::Checkbox("Viewmodels",&s.viewmodels);ImGui::TreePop();
    }
    ImGui::SliderFloat("Wetness",&s.amount,0,1);ImGui::SliderFloat("Darkening",&s.darkening,0,1);
    ImGui::SliderFloat("Water roughness",&s.roughness,.025f,1);ImGui::SliderFloat("Clearcoat",&s.coat,0,4);
    ImGui::Checkbox("Viewhands",&s.detail.viewhands);
    ImGui::Checkbox("Clearcoat entire surface",&s.detail.coatAll);
    if(ImGui::TreeNode("Droplets & flow")){
        ImGui::SliderFloat("Droplet coverage",&s.droplets,0,1);ImGui::SliderFloat("Droplet size",&s.dropSize,.1f,3,"%.2f cm");
        ImGui::SliderFloat("Position randomness",&s.detail.randomness,0,1);
        ImGui::SliderFloat("Flow displacement",&s.detail.flowDisplacement,0,2);
        ImGui::SliderFloat("Flow detail",&s.detail.flowDetail,.1f,12);
        ImGui::SliderFloat("Surface relief",&s.detail.normalStrength,0,4);
        ImGui::SliderFloat("Rivulets",&s.rivulets,0,1);ImGui::SliderFloat("Flow speed",&s.flow,0,5);ImGui::TreePop();
    }
    s.sanitize();ImGui::PopID();
}
void drawMuzzleLightControls(AppState& app){
    if(!ImGui::CollapsingHeader("Muzzle light"))return;
    ImGui::PushID("muzzle_light");auto& s=app.muzzleLight;
    ImGui::Checkbox("Enable muzzle light",&s.enabled);
    ImGui::SliderFloat("Radius",&s.radius,10,1000,"%.0f cm");ImGui::SliderFloat("Intensity",&s.intensity,0,10,"%.2fx");
    ImGui::SliderFloat("Falloff",&s.falloff,.1f,8);ImGui::Combo("Shape",&s.shape,"Sphere\0Cone\0Ellipsoid\0");
    if(s.shape==1)ImGui::SliderFloat("Cone angle",&s.cone,1,360,"%.0f deg");
    if(s.shape==2)ImGui::SliderFloat("Length ratio",&s.aspect,.1f,5);
    ImGui::PopID();
}
