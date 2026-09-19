void drawDayNightControls(AppState& app){
    if(!ImGui::CollapsingHeader("Day / night cycle"))return;
    drawDayNightPresets(app);
    auto&s=app.dayNight;
    ImGui::Checkbox("Enable day / night",&s.enabled);
    ImGui::Checkbox("Advance time",&s.running);ImGui::SameLine();ImGui::Checkbox("Rotate sun",&s.rotateSun);
    if(ImGui::SliderFloat("Start time / manual time",&s.hour,0,24,"%.2f h"))app.dayNightSeconds=0;
    if(ImGui::Button("Restart cycle"))app.dayNightSeconds=0;
    ImGui::SameLine();ImGui::Text("Now: %.2f h",render::daynight::hourAt(s,app.takePreview?app.takeTime:app.dayNightSeconds));
    ImGui::SliderFloat("Day length",&s.cycleSeconds,10,3600,"%.0f seconds",ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Day speed",&s.daySpeed,.05f,5,"%.2fx");ImGui::SliderFloat("Night speed",&s.nightSpeed,.05f,5,"%.2fx");
    ImGui::SliderFloat("Sun path azimuth",&s.azimuth,-180,180,"%.1f deg");
    if(!s.rotateSun)ImGui::SliderFloat("Manual sun elevation",&s.elevation,-90,90,"%.1f deg");
    ImGui::Checkbox("Procedural sky / clouds",&s.sky);ImGui::Checkbox("Cycle film grading",&s.film);ImGui::Checkbox("Cycle fog",&s.fog);
    if(ImGui::CollapsingHeader("Transitions")){ImGui::SliderFloat("Sun transition",&s.sunTransition,.1f,6,"%.2f hours");ImGui::SliderFloat("Film transition",&s.filmTransition,.1f,6,"%.2f hours");ImGui::SliderFloat("Fog transition",&s.fogTransition,.1f,6,"%.2f hours");}
    const char* names[]={"Day","Sunrise / sunset","Night"};
    for(int i=0;i<3;++i){ImGui::PushID(i);if(ImGui::CollapsingHeader(names[i])){auto&t=s.states[i];ImGui::SliderFloat("Sun / moon intensity",&t.sunlight,0,5);ImGui::ColorEdit3("Light color",&t.sun.x,ImGuiColorEditFlags_HDR|ImGuiColorEditFlags_Float);ImGui::SliderFloat("Ambient intensity",&t.ambient,0,2);ImGui::ColorEdit3("Ambient color",&t.ambientColor.x);ImGui::SliderFloat("Brightness",&t.brightness,-.5f,.5f);ImGui::SliderFloat("Contrast",&t.contrast,0,3);ImGui::SliderFloat("Desaturation",&t.desaturation,-1,1);ImGui::ColorEdit3("Dark tint",&t.dark.x,ImGuiColorEditFlags_HDR|ImGuiColorEditFlags_Float);ImGui::ColorEdit3("Light tint",&t.light.x,ImGuiColorEditFlags_HDR|ImGuiColorEditFlags_Float);ImGui::SliderFloat("Fog amount",&t.fogAmount,0,1);ImGui::ColorEdit3("Fog color",&t.fog.x);ImGui::SliderFloat("Fog start",&t.fogStart,0,1000,"%.1f m");ImGui::SliderFloat("Fog half distance",&t.fogHalfDistance,1,1000,"%.1f m",ImGuiSliderFlags_Logarithmic);}ImGui::PopID();}
    if(ImGui::CollapsingHeader("Sky atmosphere")){ImGui::SliderFloat("Sky intensity",&s.skyIntensity,0,5);ImGui::SliderFloat("Sun disk intensity",&s.sunDisk,0,5);ImGui::SliderFloat("Stars",&s.starIntensity,0,3);ImGui::SliderFloat("Rayleigh scattering",&s.rayleigh,.1f,5);ImGui::SliderFloat("Mie scattering",&s.mie,0,5);ImGui::SliderFloat("Mie directionality",&s.mieDirection,0,.98f);ImGui::ColorEdit3("Sunset scattering color",&s.sunsetColor.x);}
    if(ImGui::CollapsingHeader("Milky Way",ImGuiTreeNodeFlags_DefaultOpen)){
        auto&n=app.nightSky;
        const char* algorithms[]={"Fractal clouds","Ridged dust","Domain-warped clouds","Filamentary wisps","ESO photographic panorama"};
        ImGui::Combo("Milky Way source",&n.galaxyNoiseAlgorithm,algorithms,5);
        ImGui::SliderFloat("Milky Way brightness",&n.milkyWay,0,3);
        ImGui::SliderFloat("Milky Way contrast",&n.galaxyNoiseContrast,.25f,4,"%.2fx");
        ImGui::SliderFloat("Sky tilt",&n.galaxyTilt,-90,90,"%.1f deg");
        ImGui::SliderFloat("Sky azimuth",&n.galaxyAzimuth,-180,180,"%.1f deg");
        if(n.galaxyNoiseAlgorithm==4){ImGui::TextUnformatted("Milky Way panorama: ESO/S. Brunier (CC BY 4.0)");ImGui::TextWrapped("Include ESO/S. Brunier in credits when publishing captures with this sky.");}
        else {ImGui::SliderFloat("Noise scale",&n.galaxyNoiseScale,.2f,6,"%.2fx");ImGui::SliderFloat("Dust detail",&n.galaxyNoiseDetail,.25f,3,"%.2fx");ImGui::SliderFloat("Band width",&n.galaxyWidth,3,35,"%.1f deg");}
        n.sanitize();
    }
    if(ImGui::CollapsingHeader("Night sky / photography")){
        auto&n=app.nightSky;
        ImGui::Checkbox("Photographic night sky",&n.enabled);
        if(ImGui::Button("Reset night-sky controls"))n={};


        ImGui::SliderFloat("Night sky exposure",&n.exposure,-4,6,"%.2f EV");
        ImGui::SliderFloat("Sky shadow floor",&n.shadowLift,0,.3f,"%.3f");
        ImGui::SliderFloat("Night sky saturation",&n.saturation,0,2);
        ImGui::ColorEdit3("Zenith color",&n.zenithR,ImGuiColorEditFlags_Float);
        ImGui::SliderFloat("Horizon glow / light pollution",&n.horizonGlow,0,3);
        ImGui::ColorEdit3("Horizon color",&n.horizonR,ImGuiColorEditFlags_Float);
        ImGui::SliderFloat("Night cloud illumination",&n.cloudLight,0,5);
        if(ImGui::CollapsingHeader("Star appearance")){ImGui::SliderFloat("Star density",&n.starDensity,0,1);ImGui::SliderFloat("Star size",&n.starSize,.4f,2);ImGui::SliderFloat("Star color variation",&n.starColor,0,1);ImGui::SliderFloat("Star glow",&n.starGlow,0,2);}

        if(ImGui::CollapsingHeader("Moon / sky only")){ImGui::SliderFloat("Moon brightness",&n.moonIntensity,0,5);ImGui::SliderFloat("Moon diameter",&n.moonSize,.1f,3,"%.2f deg");ImGui::SliderFloat("Illuminated fraction",&n.moonPhase,0,1);ImGui::SliderFloat("Moon elevation",&n.moonElevation,-10,90,"%.1f deg");ImGui::SliderFloat("Moon azimuth",&n.moonAzimuth,-180,180,"%.1f deg");ImGui::SliderFloat("Moon halo",&n.moonHalo,0,3);}
        n.sanitize();
    }
    if(ImGui::CollapsingHeader("Long-exposure stars")){if(!s.running){ImGui::Checkbox("Auto-rotate stars",&s.starAutoRotate);if(s.starAutoRotate)ImGui::SliderFloat("Star rotation speed",&s.starRotationSpeed,-30,30,"%.2f deg/sec");}ImGui::SliderFloat("Trail length",&s.starTrailLength,0,180,"%.1f deg");ImGui::SliderFloat("Trail brightness",&s.starTrailBrightness,0,10);ImGui::SliderFloat("Tail fade",&s.starTrailFade,0,5);ImGui::SliderFloat("Pole elevation",&s.starPoleElevation,-90,90,"%.1f deg");ImGui::SliderFloat("Pole azimuth",&s.starPoleAzimuth,-180,180,"%.1f deg");}
    if(ImGui::CollapsingHeader("Clouds")){ImGui::SliderFloat("Coverage",&s.coverage,0,1);ImGui::SliderFloat("Thickness",&s.thickness,.05f,2);ImGui::SliderFloat("Absorption",&s.absorption,.05f,8);ImGui::SliderFloat("Cloud scale",&s.cloudScale,.1f,10);ImGui::SliderFloat("Cloud exposure",&s.cloudExposure,0,5);ImGui::SliderFloat("Wind angle",&s.windAngle,-180,180,"%.0f deg");ImGui::SliderFloat("Wind speed",&s.windSpeed,0,.2f);ImGui::SliderInt("Cloud steps",&s.cloudSteps,4,32);ImGui::SliderInt("Cloud lighting steps",&s.lightSteps,0,4);}
    s.sanitize();
}
