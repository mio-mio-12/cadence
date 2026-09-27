namespace {
struct WeatherGlState {
    GLint fb{},readFb{},active{},program{},vao{},viewport[4]{},textures[2]{},srcRgb{},dstRgb{},srcAlpha{},dstAlpha{};
    GLboolean depthMask{};bool blend{},depth{},cull{};
    WeatherGlState(){
        glGetIntegerv(0x8CA6,&fb);glGetIntegerv(0x8CAA,&readFb);glGetIntegerv(0x84E0,&active);glGetIntegerv(0x8B8D,&program);glGetIntegerv(0x85B5,&vao);glGetIntegerv(GL_VIEWPORT,viewport);
        glGetIntegerv(0x80C9,&srcRgb);glGetIntegerv(0x80C8,&dstRgb);glGetIntegerv(0x80CB,&srcAlpha);glGetIntegerv(0x80CA,&dstAlpha);
        glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);blend=glIsEnabled(GL_BLEND);depth=glIsEnabled(GL_DEPTH_TEST);cull=glIsEnabled(GL_CULL_FACE);
        for(int i=0;i<2;++i){glapi::ActiveTexture(glapi::Texture0+i);glGetIntegerv(GL_TEXTURE_BINDING_2D,&textures[i]);}
    }
    ~WeatherGlState(){
        for(int i=0;i<2;++i){glapi::ActiveTexture(glapi::Texture0+i);glBindTexture(GL_TEXTURE_2D,textures[i]);}
        glapi::ActiveTexture(active);glapi::UseProgram(program);glapi::BindVertexArray(vao);glapi::BindFramebuffer(glapi::DrawFramebuffer,fb);glapi::BindFramebuffer(glapi::ReadFramebuffer,readFb);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
        glapi::BlendFuncSeparate(srcRgb,dstRgb,srcAlpha,dstAlpha);glDepthMask(depthMask);
        for(auto p:{std::pair{GL_BLEND,blend},std::pair{GL_DEPTH_TEST,depth},std::pair{GL_CULL_FACE,cull}}){if(p.second)glEnable(p.first);else glDisable(p.first);}
    }
};
}
void StageRenderer::configureWeatherCloud(unsigned program){
    const auto& c=weather_.cloud;const auto loc=[&](const char* n){return uniformLocation(program,n);};
    const bool enabled=c.enabled&&debugView_==0&&!viewmodelCapture_;
    glapi::Uniform1i(loc("uWeatherCloud"),enabled);if(!enabled)return;
    const auto drift=weather::drift(c.ownWind?c.wind:weather_.wind,weatherTime_);
    const auto sun=scene::normalize(sunDirection_)*-1.f;
    glapi::Uniform4f(loc("uWeatherCloudShape"),weather::safe(c.size,1,1e7f),weather::safe(c.coverage,0,1),weather::safe(std::abs(c.softness),.0001f,2),weather::safe(c.breakup,0,1));
    glapi::Uniform4f(loc("uWeatherCloudMotion"),drift.x,drift.y,weather::safe(c.height,-1e7f,1e7f),float(weather_.seed%10000)*.013f);
    glapi::Uniform3f(loc("uWeatherCloudSun"),sun.x,sun.y,c.dayNight?sun.z:std::max(.1f,std::abs(sun.z)));
    glapi::Uniform1f(loc("uWeatherCloudStrength"),weather::safe(c.strength,0,1));
}
void StageRenderer::renderWeather(const scene::Mat4& vp){
    if(debugView_!=0||viewmodelCapture_||!weatherError_.empty())return;
    const bool particles=weather_.snow.enabled||weather_.dust.enabled||weather_.debris.enabled;
    if(!particles&&!weather_.fog.enabled)return;
    if(particles&&!weatherProgram_){weatherProgram_=compileProgram(kWeatherVertex.c_str(),kWeatherParticleFragment.c_str(),weatherError_);if(!weatherProgram_)return;}
    if(weather_.fog.enabled&&!weatherScreenProgram_){weatherScreenProgram_=compileProgram(kEnvironmentVertex,kWeatherScreenFragment.c_str(),weatherError_);if(!weatherScreenProgram_)return;}
    WeatherGlState restore;
    if(!weatherFramebuffer_)glapi::GenFramebuffers(1,&weatherFramebuffer_);
    glapi::BindFramebuffer(glapi::Framebuffer,weatherFramebuffer_);glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::ColorAttachment0,GL_TEXTURE_2D,colorTexture_,0);
    if(glapi::CheckFramebufferStatus(glapi::Framebuffer)!=glapi::FramebufferComplete){weatherError_="Weather framebuffer is incomplete";return;}
    glViewport(0,0,width_,height_);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDepthMask(GL_FALSE);glEnable(GL_BLEND);glapi::BlendFuncSeparate(GL_ONE,GL_ONE_MINUS_SRC_ALPHA,GL_ZERO,GL_ONE);glapi::BindVertexArray(lineVao_);
    const auto right=scene::normalize(scene::Vec3{vp.v[0],vp.v[4],vp.v[8]}),up=scene::normalize(scene::Vec3{vp.v[1],vp.v[5],vp.v[9]});
    const float time=float(std::remainder(weatherTime_,100000.));
    const auto common=[&](unsigned program){
        glapi::UseProgram(program);const auto loc=[&](const char* n){return uniformLocation(program,n);};
        glapi::Uniform2f(loc("uViewport"),float(width_),float(height_));glapi::Uniform2f(loc("uRange"),cameraNear_,cameraFar_);
        glapi::Uniform3f(loc("uCamera"),cameraPosition_.x,cameraPosition_.y,cameraPosition_.z);glapi::Uniform3f(loc("uRight"),right.x,right.y,right.z);glapi::Uniform3f(loc("uUp"),up.x,up.y,up.z);
        glapi::Uniform1f(loc("uTime"),time);glapi::Uniform1f(loc("uSeed"),float(weather_.seed%10000));glapi::Uniform1i(loc("uForeground"),firstPersonProjection_&&foregroundDrawn_);
        glapi::Uniform1i(loc("uDepth"),0);glapi::ActiveTexture(glapi::Texture0);glBindTexture(GL_TEXTURE_2D,depthBuffer_);
    };
    if(particles){
        common(weatherProgram_);const auto loc=[&](const char* n){return uniformLocation(weatherProgram_,n);};
        glapi::UniformMatrix4fv(loc("uVP"),1,GL_FALSE,vp.data());glapi::Uniform1f(loc("uRadius"),weather::safe(weather_.range,100,20000));
        glapi::Uniform1i(loc("uShelter"),1);glapi::ActiveTexture(glapi::Texture0+1);glBindTexture(GL_TEXTURE_2D,weatherShelter_.texture);
        glapi::Uniform3f(loc("uTile"),weatherShelter_.center.x,weatherShelter_.center.y,std::max(1.f,weatherShelter_.extent));glapi::Uniform3f(loc("uShelterSlope"),0,0,0);
        glapi::Uniform3f(loc("uLight"),weather::safe(ambientColor_.x*sunAmbient_+sunColor_.x*sunIntensity_*.35f,.05f,10),weather::safe(ambientColor_.y*sunAmbient_+sunColor_.y*sunIntensity_*.35f,.05f,10),weather::safe(ambientColor_.z*sunAmbient_+sunColor_.z*sunIntensity_*.35f,.05f,10));
        glapi::Uniform1i(loc("uFog"),fogEnabled_);glapi::Uniform4f(loc("uFogParams"),fogStart_,fogHalfDistance_,fogOpacity_,0);glapi::Uniform3f(loc("uFogColor"),fogColor_.x,fogColor_.y,fogColor_.z);
        const auto draw=[&](const weather::Particles& p,int kind){
            if(!p.enabled)return;const auto& w=p.ownWind?p.wind:weather_.wind;const auto drift=weather::drift(w,weatherTime_);
            glapi::Uniform3f(loc("uDrift"),drift.x,drift.y,0);glapi::Uniform1f(loc("uFall"),float(std::remainder(weatherTime_*weather::safe(p.speed,-100,100)*100.,1.e8)));
            glapi::Uniform4f(loc("uTurbulence"),weather::safe(w.turbulence,-20,20)*30,weather::safe(std::abs(w.scale),.01f,10000)*100,weather::safe(w.rate,-100,100),0);
            glapi::Uniform4f(loc("uHeight"),weather::safe(std::min(p.bottom,p.top),-1e7f,1e7f),weather::safe(std::max(p.bottom,p.top),-1e7f,1e7f),30,0);
            const auto budget=weather::particleBudget(kind==3?p.sheets:p.density,weather_.quality,kind);
            glapi::Uniform1i(loc("uSlots"),budget.slots);
            glapi::Uniform1f(loc("uResponse"),weather::safe(p.response,-10,10));glapi::Uniform1f(loc("uDensity"),budget.probability);
            glapi::Uniform3f(loc("uAerosol"),weather::safe(weather_.dustAerosolDetail,.0001f,400),weather::safe(weather_.dustAerosolAmount,0,1),weather::safe(weather_.dustAerosolContrast,.0001f,5));
            glapi::Uniform1f(loc("uSize"),weather::safe(std::abs(kind==3?p.sheetSize:p.size),.01f,1000));glapi::Uniform1f(loc("uVariation"),weather::safe(p.variation,0,1));
            glapi::Uniform1f(loc("uFlutter"),weather::safe(p.flutter,-100,100));glapi::Uniform1f(loc("uTumble"),weather::safe(p.tumble,-100,100));
            glapi::Uniform1f(loc("uOpacity"),weather::safe(p.opacity*(kind==3?.4f:1.f),0,1));glapi::Uniform1f(loc("uBrightness"),weather::safe(p.brightness,0,100));
            glapi::Uniform1f(loc("uSoftness"),weather::safe(p.softness,.01f,10000));glapi::Uniform1f(loc("uNear"),weather::safe(p.nearFade,0,10000));
            glapi::Uniform3f(loc("uColor"),weather::safe(p.color.x,0,100),weather::safe(p.color.y,0,100),weather::safe(p.color.z,0,100));
            glapi::Uniform1i(loc("uKind"),kind);glapi::Uniform1i(loc("uStyle"),p.style);glapi::Uniform1i(loc("uSheltered"),p.shelter&&weatherShelter_.ready);
            glapi::DrawArrays(GL_TRIANGLES,0,13*13*13*budget.slots*6);
        };
        // Coarse haze first; grains and flakes retain their sharper silhouettes.
        if(weather_.dust.sheets>0)draw(weather_.dust,3);draw(weather_.dust,1);draw(weather_.snow,0);draw(weather_.debris,2);
    }
    if(weather_.fog.enabled){
        common(weatherScreenProgram_);const auto loc=[&](const char* n){return uniformLocation(weatherScreenProgram_,n);};const auto& f=weather_.fog;
        const auto forward=scene::normalize(scene::Vec3{vp.v[3],vp.v[7],vp.v[11]});glapi::Uniform3f(loc("uForward"),forward.x,forward.y,forward.z);
        const float px=scene::length({vp.v[0],vp.v[4],vp.v[8]}),py=scene::length({vp.v[1],vp.v[5],vp.v[9]});glapi::Uniform2f(loc("uInvProjection"),1/std::max(.001f,px),1/std::max(.001f,py));
        glapi::Uniform1i(loc("uMode"),0);glapi::Uniform1i(loc("uSteps"),weather_.quality==0?12:weather_.quality==1?24:48);
        const float bottom=weather::safe(std::min(f.bottom,f.top),-1e7f,1e7f),top=std::max(bottom+.001f,weather::safe(std::max(f.bottom,f.top),-1e7f,1e7f));
        glapi::Uniform4f(loc("uA"),bottom,top,weather::safe(f.density,0,1000),weather::safe(f.opacity,0,1));
        glapi::Uniform4f(loc("uB"),weather::safe(std::abs(f.size),1,1e7f),weather::safe(f.breakup,0,1),weather::safe(f.distance,1,1e7f),weather::safe(f.start,0,1e7f));
        glapi::Uniform4f(loc("uC"),std::min((top-bottom)*.49f,weather::safe(std::abs(f.edge),.0001f,1e6f)),0,0,0);
        const auto drift=weather::drift(weather_.wind,weatherTime_*weather::safe(f.drift,-100,100));glapi::Uniform3f(loc("uDrift"),drift.x,drift.y,0);
        glapi::Uniform3f(loc("uColor"),weather::safe(f.color.x,0,100),weather::safe(f.color.y,0,100),weather::safe(f.color.z,0,100));
        glapi::DrawArrays(GL_TRIANGLES,0,3);
    }
}
// Kept as a no-op for older diagnostic callers; lens weather is removed.
void StageRenderer::finishLensWeather(){}
