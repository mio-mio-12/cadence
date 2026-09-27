bool loadWeatherPreset(AppState& app,const std::filesystem::path& path){
    std::ifstream in(path);std::string signature;int version{};render::weather::Settings candidate;
    if(!(in>>signature>>version)||signature!="CASTWEATHER"||version<1||version>3||!(in>>candidate))return false;
    if(version>=2&&!(in>>candidate.dustAerosolDetail>>candidate.dustAerosolAmount>>candidate.dustAerosolContrast))return false;
    auto rain=app.rain;auto wet=app.wet;
    if(version>=3&&!(in>>rain>>rain.style>>rain.blockerDistance>>wet>>wet.detail))return false;
    if(!std::isfinite(rain.blockerDistance))return false;
    candidate.sanitize();app.weather=candidate;if(version>=3){app.rain=rain;app.wet=wet;}return true;
}
bool saveWeatherPreset(const AppState& app,const std::filesystem::path& path){
    std::error_code ec;std::filesystem::create_directories(path.parent_path(),ec);if(ec)return false;
    std::ofstream out(path);out<<std::setprecision(std::numeric_limits<float>::max_digits10)<<"CASTWEATHER 3\n"<<app.weather<<' '<<app.weather.dustAerosolDetail<<' '<<app.weather.dustAerosolAmount<<' '<<app.weather.dustAerosolContrast<<'\n'<<app.rain<<'\n'<<app.rain.style<<' '<<app.rain.blockerDistance<<'\n'<<app.wet<<'\n'<<app.wet.detail<<'\n';out.flush();return bool(out);
}
void drawWeatherWind(render::weather::Wind& w){
    ImGui::SliderFloat("Speed",&w.speed,0,20,"%.2f m/s");ImGui::SliderFloat("Direction",&w.direction,-180,180,"%.1f deg");
    ImGui::SliderFloat("Gusts",&w.gust,0,2);ImGui::SliderFloat("Gust period",&w.period,.1f,30,"%.2f sec");
    ImGui::SliderFloat("Turbulence",&w.turbulence,0,5);ImGui::SliderFloat("Swirl size",&w.scale,.1f,30,"%.2f m");ImGui::SliderFloat("Swirl speed",&w.rate,0,5);
}
void drawWeatherParticles(const char* title,render::weather::Particles& p,int kind,render::weather::Settings& settings){
    if(!ImGui::TreeNode(title))return;ImGui::Checkbox("Enabled",&p.enabled);
    if(kind==2)ImGui::Combo("Shape",&p.style,"Leaves\0Ash\0Scraps\0");
    ImGui::SliderFloat("Density",&p.density,0,1);ImGui::SliderFloat("Size",&p.size,.05f,kind==2?15.f:3.f,"%.2f cm");ImGui::SliderFloat("Size variation",&p.variation,0,1);
    ImGui::SliderFloat("Fall speed",&p.speed,-3,5,"%.2f m/s");ImGui::SliderFloat("Flutter",&p.flutter,0,5);ImGui::SliderFloat("Tumble",&p.tumble,0,15);
    ImGui::SliderFloat("Wind response",&p.response,0,3);ImGui::Checkbox("Shelter",&p.shelter);
    if(kind==1){ImGui::SliderFloat("Dust sheets",&p.sheets,0,1);ImGui::SliderFloat("Sheet size",&p.sheetSize,10,600,"%.1f cm");ImGui::SliderFloat("Aerosol detail",&settings.dustAerosolDetail,1,400);ImGui::SliderFloat("Aerosol amount",&settings.dustAerosolAmount,0,1);ImGui::SliderFloat("Aerosol contrast",&settings.dustAerosolContrast,.1f,5);}
    if(ImGui::TreeNode("Appearance")){
        ImGui::ColorEdit3("Color",&p.color.x,ImGuiColorEditFlags_Float|ImGuiColorEditFlags_HDR);
        ImGui::SliderFloat("Opacity",&p.opacity,0,1);ImGui::SliderFloat("Brightness",&p.brightness,0,5);ImGui::SliderFloat("Soft intersections",&p.softness,.1f,100,"%.1f cm");
        ImGui::SliderFloat("Camera clearance",&p.nearFade,0,200,"%.1f cm");ImGui::DragFloat("Bottom",&p.bottom,1,0,0,"%.1f cm");ImGui::DragFloat("Top",&p.top,1,0,0,"%.1f cm");ImGui::TreePop();
    }
    if(ImGui::TreeNode("Wind override")){ImGui::Checkbox("Use own wind",&p.ownWind);drawWeatherWind(p.wind);ImGui::TreePop();}ImGui::TreePop();
}
void drawWeatherControls(AppState& app){
    if(!ImGui::CollapsingHeader("Weather"))return;ImGui::PushID("experimental_weather");auto& s=app.weather;
    const auto directory=cadenceAssetsDirectory()/"weather_presets";
    const auto files=uiFileLists().get(directory,"|.castweather|",false,false);
    static constexpr const char* looks[]={"Gentle snow","Blizzard","Dust gusts","Sandstorm","Ground mist","Cloud shadows","","Autumn leaves"};
    if(ImGui::BeginCombo("Preset",app.weatherSelectedPreset.empty()?"Choose a look":app.weatherSelectedPreset.c_str())){
        for(int n=0;n<8;++n)if(n!=6&&ImGui::Selectable(looks[n])){s=render::weather::preset(n);app.weatherSelectedPreset=looks[n];}
        for(const auto& path:*files)if(ImGui::Selectable(path.stem().string().c_str())){if(loadWeatherPreset(app,path)){app.weatherSelectedPreset=path.stem().string();std::snprintf(app.weatherPresetName.data(),app.weatherPresetName.size(),"%s",path.stem().string().c_str());}else app.status="Could not load weather preset";}
        ImGui::EndCombo();
    }
    ImGui::InputText("Name",app.weatherPresetName.data(),app.weatherPresetName.size());
    std::string name=app.weatherPresetName.data();for(auto& c:name)if(!std::isalnum(static_cast<unsigned char>(c))&&c!='-'&&c!='_')c='_';
    ImGui::BeginDisabled(name.empty());if(ImGui::Button("Save")){if(saveWeatherPreset(app,directory/(name+".castweather"))){app.weatherSelectedPreset=name;uiFileLists().get(directory,"|.castweather|",false,true);}else app.status="Could not save weather preset";}
    ImGui::SameLine();ImGui::EndDisabled();if(ImGui::Button("Save as default")){app.status=saveWeatherPreset(app,directory/"default.castweather")?"Saved weather default":"Could not save weather default";uiFileLists().get(directory,"|.castweather|",false,true);}
    ImGui::SameLine();ImGui::BeginDisabled(name.empty());if(ImGui::Button("Delete")){std::error_code ec;std::filesystem::remove(directory/(name+".castweather"),ec);app.status=ec?"Could not delete weather preset":"Deleted weather preset";uiFileLists().get(directory,"|.castweather|",false,true);app.weatherSelectedPreset.clear();}ImGui::EndDisabled();
    ImGui::Indent();
    drawRainControls(app);
    drawWetControls(app);
    if(ImGui::TreeNode("Shared wind")){drawWeatherWind(s.wind);ImGui::TreePop();}
    if(ImGui::TreeNode("Cloud shadows")){
        auto& c=s.cloud;ImGui::Checkbox("Enabled",&c.enabled);ImGui::SliderFloat("Coverage",&c.coverage,0,1);ImGui::SliderFloat("Strength",&c.strength,0,1);ImGui::SliderFloat("Softness",&c.softness,.001f,.5f);
        ImGui::SliderFloat("Size",&c.size,50,10000,"%.0f cm");ImGui::SliderFloat("Breakup",&c.breakup,0,1);ImGui::DragFloat("Altitude",&c.height,5,0,0,"%.0f cm");ImGui::Checkbox("Follow day / night",&c.dayNight);
        if(ImGui::TreeNode("Wind override")){ImGui::Checkbox("Use own wind",&c.ownWind);drawWeatherWind(c.wind);ImGui::TreePop();}ImGui::TreePop();
    }
    if(ImGui::TreeNode("Ground fog")){
        auto& f=s.fog;ImGui::Checkbox("Enabled",&f.enabled);ImGui::DragFloat("Bottom",&f.bottom,1,0,0,"%.1f cm");ImGui::DragFloat("Top",&f.top,1,0,0,"%.1f cm");
        if(ImGui::Button("Place at camera")){f.bottom=app.lastRenderedCameraPosition.z-170;f.top=app.lastRenderedCameraPosition.z-50;}
        ImGui::SliderFloat("Density",&f.density,0,3);ImGui::SliderFloat("Opacity",&f.opacity,0,1);ImGui::ColorEdit3("Color",&f.color.x,ImGuiColorEditFlags_Float|ImGuiColorEditFlags_HDR);
        ImGui::SliderFloat("Bank size",&f.size,10,5000,"%.0f cm");ImGui::SliderFloat("Breakup",&f.breakup,0,1);ImGui::SliderFloat("Edge softness",&f.edge,1,300,"%.0f cm");ImGui::SliderFloat("Drift",&f.drift,0,2);
        ImGui::SliderFloat("Start distance",&f.start,0,1000,"%.0f cm");ImGui::SliderFloat("Range",&f.distance,100,20000,"%.0f cm");ImGui::TreePop();
    }
    drawWeatherParticles("Snow",s.snow,0,s);drawWeatherParticles("Dust / sand",s.dust,1,s);drawWeatherParticles("Debris",s.debris,2,s);
   if(ImGui::TreeNode("Timing & quality")){
        if(ImGui::Checkbox("Animate",&s.animate)&&!s.animate)s.freezeTime=float(app.takePreview?app.takeTime:app.weatherSeconds);
        ImGui::SliderFloat("Speed",&s.timeScale,0,4);ImGui::DragFloat("Time offset",&s.offset,.01f,0,0,"%.3f sec");ImGui::InputInt("Seed",&s.seed);
        ImGui::Combo("Quality",&s.quality,"Low\0High\0Cinematic\0");ImGui::SliderFloat("Particle range",&s.range,300,5000,"%.0f cm");ImGui::SliderFloat("Blocker distance",&s.blockerDistance,0,20000,"%.0f cm");ImGui::TreePop();
    }
    ImGui::Unindent();s.sanitize();if(!app.renderer.weatherError().empty())ImGui::TextWrapped("%s",app.renderer.weatherError().c_str());ImGui::PopID();
}
