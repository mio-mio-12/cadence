void drawVisualRandomizer(AppState& app){
    if(!ImGui::CollapsingHeader("Visual preset randomizer"))return;
    static float similarity=75;static bool lighting=true,colors=true,materials=false,atmosphere=true,post=true,skybox=false;static int seed=1;
    static cadence::VisualRandomBaseline baseline;
    static std::optional<std::filesystem::path> baseSky;
    static std::uint64_t revision{};
    if(revision!=app.visualSettingsRevision){baseline.clear();baseSky.reset();revision=app.visualSettingsRevision;}
    bool changed=ImGui::SliderFloat("Similarity to base",&similarity,0,100,"%.0f%%");
    changed|=ImGui::Checkbox("Lighting##random",&lighting);ImGui::SameLine();changed|=ImGui::Checkbox("Colors##random",&colors);
    changed|=ImGui::Checkbox("Materials##random",&materials);ImGui::SameLine();changed|=ImGui::Checkbox("Atmosphere##random",&atmosphere);ImGui::SameLine();changed|=ImGui::Checkbox("Post effects##random",&post);
    changed|=ImGui::Checkbox("Skybox##random",&skybox);
    changed|=ImGui::SliderInt("Seed##random",&seed,0,10000);
    const bool seedActive=ImGui::IsItemActive();
    const bool seedReleased=ImGui::IsItemDeactivatedAfterEdit();
    if(ImGui::Button("Randomize")){seed=seed==INT_MAX?0:seed+1;changed=true;}
    ImGui::SameLine();if(ImGui::Button("Use current as base")){baseline.clear();baseSky=app.environmentPath;}
    const auto applySky=[&](const std::filesystem::path& path){if(path==app.environmentPath)return;std::string error;if(path.empty()){app.renderer.clearEnvironment();app.environmentPath.clear();}else if(lowerText(path.extension().string())==".iwi"?app.renderer.setT6SkyboxIwi(path,error):app.renderer.setEnvironmentPanorama(path,error))app.environmentPath=path;else app.status=error;};
    if(changed||seedReleased){
        if(!baseSky)baseSky=app.environmentPath;
        baseline.restore();std::mt19937 rng(static_cast<unsigned>(seed));const float keep=std::clamp(similarity,0.f,100.f);
        const auto f=[&](float& value,float low,float high){value=cadence::randomizedVisual(baseline.original(value),low,high,keep,rng);};
        const auto b=[&](bool& value){value=baseline.original(value);if(std::uniform_real_distribution<float>(0,100)(rng)>=keep)value=(rng()&1)!=0;};
        const auto color=[&](scene::Vec3& v){f(v.x,0,1);f(v.y,0,1);f(v.z,0,1);};
        if(lighting){b(app.sunLighting);f(app.sunAzimuth,0,360);f(app.sunElevation,5,85);f(app.sunIntensity,0,3);f(app.sunAmbient,.05f,1);f(app.environmentIntensity,0,3);f(app.environmentExposure,-2,2);f(app.environmentRotation,0,360);f(app.cubemapSpecularIntensity,0,2);f(app.weaponSunMultiplier,.1f,2);f(app.weaponAmbientMultiplier,.1f,2);}
        if(colors){color(app.sunColor);color(app.ambientColor);color(app.fogColor);b(app.filmTweaks);f(app.filmBrightness,-.15f,.15f);f(app.filmContrast,.65f,1.4f);f(app.filmDesaturation,0,1);color(app.filmDarkTint);color(app.filmLightTint);}
        if(materials){f(app.awRoughnessScale,.4f,2);f(app.awRoughnessBias,-.2f,.2f);f(app.awMetalness,0,1);f(app.awSpecularLevel,0,1);f(app.awClearcoat,0,1);f(app.eeveeMetallic,0,1);f(app.eeveeRoughness,.05f,1);f(app.eeveeSpecular,0,1);f(app.eeveeClearcoat,0,1);f(app.specularIntensity,0,2);}
        if(atmosphere){b(app.fogEnabled);f(app.fogStartMeters,0,80);f(app.fogHalfDistanceMeters,10,300);f(app.fogOpacity,0,1);b(app.rain.enabled);f(app.rain.density,0,1);f(app.rain.wind,0,8);f(app.rain.direction,-180,180);f(app.rain.opacity,.1f,.8f);f(app.rain.turbulence,0,1);color(app.rain.tint);}
        if(post){b(app.bloomEnabled);f(app.bloomIntensity,0,1.5f);f(app.bloomThreshold,.5f,3);f(app.bloomRadius,1,8);b(app.vignetteEnabled);f(app.vignetteIntensity,0,.8f);f(app.vignetteRadius,.3f,1);f(app.vignetteSoftness,.1f,1);b(app.hbao.enabled);f(app.hbao.intensity,0,3);f(app.hbao.radius,10,150);f(app.lutIntensity,0,1);}
        if(colors){f(app.sunKelvin,2500,10000);f(app.ambientKelvin,2500,10000);}
        if(atmosphere){
            for(auto* p:{&app.weather.snow,&app.weather.dust,&app.weather.debris}){b(p->enabled);f(p->density,0,1);f(p->opacity,.1f,1);f(p->variation,0,1);f(p->flutter,0,2);color(p->color);}
            b(app.weather.fog.enabled);f(app.weather.fog.opacity,0,1);f(app.weather.fog.density,.05f,1);f(app.weather.fog.breakup,0,1);color(app.weather.fog.color);
            b(app.weather.cloud.enabled);f(app.weather.cloud.coverage,.1f,.9f);f(app.weather.cloud.strength,0,1);
        }
        app.status="Visuals randomized; save with the visual preset controls";
        // Loading an environment builds GPU resources. Commit on release,
        // not for every intermediate seed while the slider is dragged.
        if(!seedActive){auto path=*baseSky;if(skybox&&keep<100){std::mt19937 skyRng(static_cast<unsigned>(seed)^0x534b59u);const auto skies=uiFileLists().get(programDirectory()/"skyboxes","|.iwi|.hdr|.exr|.png|.jpg|.jpeg|",true);if(!skies->empty()&&std::uniform_real_distribution<float>(0,100)(skyRng)>=keep)path=(*skies)[std::uniform_int_distribution<std::size_t>(0,skies->size()-1)(skyRng)];}applySky(path);}
    }
    ImGui::SameLine();ImGui::BeginDisabled(baseline.empty()&&!baseSky);if(ImGui::Button("Undo randomize")){baseline.restore();if(baseSky)applySky(*baseSky);baseline.clear();baseSky.reset();}ImGui::EndDisabled();
}
