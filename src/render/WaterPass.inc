// Real depth-writing geometry, before transparent world surfaces and viewhands.
void StageRenderer::renderWater(const scene::Mat4& vp,const scene::Mat4& light,const scene::Mat4& farLight){
    if(!water_.enabled||viewmodelCapture_)return;
    if(!waterProgram_){
        if(!waterError_.empty())return;
        waterProgram_=compileProgram(kWaterVertex,kWaterFragment,waterError_);if(!waterProgram_)return;
    }
    // Snapshot opaque/decal color and depth before drawing the water, including
    // MSAA resolve. Match stencil depth formats, otherwise depth blits are illegal.
    GLint previousActive=0,previous16=0,previous17=0,previous18=0,previous19=0,drawFbo=0,readFbo=0;
    glGetIntegerv(0x84E0,&previousActive);
    glapi::ActiveTexture(glapi::Texture0+16);glGetIntegerv(GL_TEXTURE_BINDING_2D,&previous16);
    glapi::ActiveTexture(glapi::Texture0+17);glGetIntegerv(GL_TEXTURE_BINDING_2D,&previous17);
    glapi::ActiveTexture(glapi::Texture0+18);glGetIntegerv(GL_TEXTURE_BINDING_2D,&previous18);
    glapi::ActiveTexture(glapi::Texture0+19);glGetIntegerv(GL_TEXTURE_BINDING_2D,&previous19);
    glGetIntegerv(0x8CA6,&drawFbo);glGetIntegerv(0x8CAA,&readFbo);
    bool depthReady=water_.optics.depthEnabled&&debugView_==0;
    if(depthReady){
        const bool stencil=overlayTargetStencil_;
        if(!waterSceneFramebuffer_)glapi::GenFramebuffers(1,&waterSceneFramebuffer_);
        if(!waterSceneColor_)glGenTextures(1,&waterSceneColor_);
        if(!waterSceneDepth_)glGenTextures(1,&waterSceneDepth_);
        if(waterSceneWidth_!=width_||waterSceneHeight_!=height_||waterSceneStencil_!=stencil){
            glBindTexture(GL_TEXTURE_2D,waterSceneColor_);
            glTexImage2D(GL_TEXTURE_2D,0,glapi::Rgba16f,width_,height_,0,GL_RGBA,GL_FLOAT,nullptr);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,glapi::ClampToEdge);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,glapi::ClampToEdge);
            glBindTexture(GL_TEXTURE_2D,waterSceneDepth_);
            glTexImage2D(GL_TEXTURE_2D,0,stencil?0x8CAD:glapi::DepthComponent32f,width_,height_,0,stencil?0x84F9:GL_DEPTH_COMPONENT,stencil?0x8DAD:GL_FLOAT,nullptr);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,glapi::ClampToEdge);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,glapi::ClampToEdge);
            waterSceneWidth_=width_;waterSceneHeight_=height_;waterSceneStencil_=stencil;
        }
        glapi::BindFramebuffer(glapi::Framebuffer,waterSceneFramebuffer_);
        glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::ColorAttachment0,GL_TEXTURE_2D,waterSceneColor_,0);
        glapi::FramebufferTexture2D(glapi::Framebuffer,0x821A,GL_TEXTURE_2D,0,0);
        glapi::FramebufferTexture2D(glapi::Framebuffer,stencil?0x821A:glapi::DepthAttachment,GL_TEXTURE_2D,waterSceneDepth_,0);
        depthReady=glapi::CheckFramebufferStatus(glapi::Framebuffer)==glapi::FramebufferComplete;
        if(depthReady){
            glapi::BindFramebuffer(glapi::ReadFramebuffer,drawFbo);
            glapi::BlitFramebuffer(0,0,width_,height_,0,0,width_,height_,GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT,GL_NEAREST);
            
        }
        glapi::BindFramebuffer(glapi::DrawFramebuffer,drawFbo);glapi::BindFramebuffer(glapi::ReadFramebuffer,readFbo);
    }
    glapi::ActiveTexture(glapi::Texture0+16);glBindTexture(GL_TEXTURE_2D,waterSceneDepth_);
    glapi::ActiveTexture(glapi::Texture0+17);glBindTexture(GL_TEXTURE_2D,waterSceneColor_);
    glapi::ActiveTexture(glapi::Texture0+18);
    if(!waterCrestTexture_)glGenTextures(1,&waterCrestTexture_);
    glBindTexture(GL_TEXTURE_2D,waterCrestTexture_);
    if(waterCrestTightness_!=water_.surface.tightness){
        const auto profile=water::crestTable(water_.surface.tightness);
        glTexImage2D(GL_TEXTURE_2D,0,glapi::Rgba32f,water::profileSize,1,0,GL_RGBA,GL_FLOAT,profile.data());
        if(glapi::GenerateMipmap)glapi::GenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,glapi::GenerateMipmap?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,glapi::ClampToEdge);
        waterCrestTightness_=water_.surface.tightness;
    }
    glapi::ActiveTexture(glapi::Texture0+19);
    if(!waterFoamTexture_){
        glGenTextures(1,&waterFoamTexture_);glBindTexture(GL_TEXTURE_2D,waterFoamTexture_);
        const auto foam=water::foamTexture();
        glTexImage2D(GL_TEXTURE_2D,0,glapi::Rgba8,water::foamTextureSize,water::foamTextureSize,0,GL_RGBA,GL_UNSIGNED_BYTE,foam.data());
        if(glapi::GenerateMipmap)glapi::GenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,glapi::GenerateMipmap?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    }
    glBindTexture(GL_TEXTURE_2D,waterFoamTexture_);
    const int resolution=water_.quality==0?128:water_.quality==1?256:384;
    if(waterResolution_!=resolution){
        std::vector<float> vertices;std::vector<std::uint32_t> indices;
        vertices.reserve((resolution+1)*(resolution+1)*2);indices.reserve(resolution*resolution*6);
        for(int y=0;y<=resolution;++y)for(int x=0;x<=resolution;++x){vertices.push_back(2.f*x/resolution-1);vertices.push_back(2.f*y/resolution-1);}
        for(int y=0;y<resolution;++y)for(int x=0;x<resolution;++x){
            const auto a=static_cast<std::uint32_t>(y*(resolution+1)+x),b=a+1,c=a+resolution+1,d=c+1;
            indices.insert(indices.end(),{a,b,d,a,d,c});
        }
        if(!waterVao_)glapi::GenVertexArrays(1,&waterVao_);
        if(!waterVertices_)glapi::GenBuffers(1,&waterVertices_);
        if(!waterIndices_)glapi::GenBuffers(1,&waterIndices_);
        glapi::BindVertexArray(waterVao_);glapi::BindBuffer(glapi::ArrayBuffer,waterVertices_);
        glapi::BufferData(glapi::ArrayBuffer,vertices.size()*sizeof(float),vertices.data(),glapi::StaticDraw);
        glapi::EnableVertexAttribArray(0);glapi::VertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),nullptr);
        glapi::BindBuffer(glapi::ElementArrayBuffer,waterIndices_);
        glapi::BufferData(glapi::ElementArrayBuffer,indices.size()*sizeof(std::uint32_t),indices.data(),glapi::StaticDraw);
        waterIndexCount_=static_cast<int>(indices.size());waterResolution_=resolution;
    }
    glapi::UseProgram(waterProgram_);glapi::BindVertexArray(waterVao_);
    const auto loc=[&](const char* n){return uniformLocation(waterProgram_,n);};
    const auto f=[&](const char* n,float v){glapi::Uniform1f(loc(n),v);};
    const auto i=[&](const char* n,int v){glapi::Uniform1i(loc(n),v);};
    const auto v=[&](const char* n,scene::Vec3 x){glapi::Uniform3f(loc(n),x.x,x.y,x.z);};
    glapi::UniformMatrix4fv(loc("uVP"),1,GL_FALSE,vp.data());
    const double time=water_.timeOffset+(water_.animate?waterTime_*water_.speed:0);
    v("uCenter",{cameraPosition_.x,cameraPosition_.y,water_.height});f("uRadius",water_.radius*100);
    f("uResolution",float(resolution));f("uWaterHeight",water_.height);f("uWaveHeight",water_.waveHeight);
    const auto gridForward=scene::normalize(scene::Vec3{vp.v[3],vp.v[7],vp.v[11]});
    auto gridRight=scene::normalize(scene::cross(gridForward,scene::Vec3{0,0,1}));
    if(scene::length(gridRight)<.1f)gridRight=scene::normalize(scene::Vec3{vp.v[0],vp.v[4],vp.v[8]});
    const auto gridUp=scene::cross(gridRight,gridForward);
    const float gridHeight=std::max(50.f,std::abs(cameraPosition_.z-water_.height));
    const float fy=std::sqrt(vp.v[1]*vp.v[1]+vp.v[5]*vp.v[5]+vp.v[9]*vp.v[9]);
    const float fx=std::sqrt(vp.v[0]*vp.v[0]+vp.v[4]*vp.v[4]+vp.v[8]*vp.v[8]);
    const auto cameraRight=scene::normalize(scene::Vec3{vp.v[0],vp.v[4],vp.v[8]});
    const auto cameraUp=scene::normalize(scene::Vec3{vp.v[1],vp.v[5],vp.v[9]});
    // Enclose the rolled viewport in the level projector (including ultrawide).
    const float tanX=std::abs(scene::dot(cameraRight,gridRight))/std::max(.01f,fx)+std::abs(scene::dot(cameraUp,gridRight))/std::max(.01f,fy);
    const float tanY=std::abs(scene::dot(cameraRight,gridUp))/std::max(.01f,fx)+std::abs(scene::dot(cameraUp,gridUp))/std::max(.01f,fy);
    const float top=gridUp.z*tanY>.001f?std::min(1.65f,(-gridHeight/(water_.radius*100)-gridForward.z)/(gridUp.z*tanY)):1.65f;
    v("uGridForward",gridForward);v("uGridRight",gridRight);v("uGridUp",gridUp);
    glapi::Uniform2f(loc("uGridTan"),tanX,tanY);f("uGridHeight",gridHeight);f("uGridTop",std::max(-1.79f,top));
    i("uProjectedGrid",cameraPosition_.z-water_.height>50.f&&gridForward.z<.6f);
    i("uSceneDepth",16);i("uSceneColor",17);i("uDepthEnabled",depthReady);
    i("uCrestProfile",18);i("uFoamTexture",19);
    f("uFoamPersistence",water_.surface.foamPersistence);f("uFoamDetail",water_.surface.foamDetail);
    glapi::Uniform2f(loc("uViewport"),float(width_),float(height_));
    const float projectionY=std::sqrt(vp.v[1]*vp.v[1]+vp.v[5]*vp.v[5]+vp.v[9]*vp.v[9]);
    f("uPixelScale",2.f/std::max(1.f,projectionY*height_));
    glapi::Uniform2f(loc("uRange"),cameraNear_,cameraFar_);
    v("uForward",scene::normalize(scene::Vec3{vp.v[3],vp.v[7],vp.v[11]}));
    v("uShallow",water_.optics.shallow);f("uAbsorption",water_.optics.absorption);
    f("uTransmission",water_.optics.transmission);f("uCrestLight",water_.optics.crestLight);
    f("uFoamCoverage",water_.optics.foamCoverage);f("uFoamScale",water_.optics.foamScale);
    f("uShoreFoam",water_.optics.shoreFoam);f("uShoreWidth",water_.optics.shoreWidth);
    const auto waves=water::waves(water_);f("uChop",water::safeChop(waves,water_.choppiness));
    for(int index=0;index<water::waveCount;++index){
        const auto& w=waves[index];const auto name="uWaves["+std::to_string(index)+"]",phase="uPhases["+std::to_string(index)+"]";
        glapi::Uniform4f(loc(name.c_str()),w.x,w.y,w.amplitude,w.k);
        f(phase.c_str(),static_cast<float>(std::remainder(w.phase-time*w.omega,2.0*scene::kPi)));
    }
    v("uCamera",cameraPosition_);v("uColor",water_.color);v("uSun",sunDirection_);
    v("uSunColor",sunEnabled_?sunColor_*sunIntensity_:scene::Vec3{});
    v("uAmbient",sunEnabled_?ambientColor_*sunAmbient_:scene::Vec3{.6f,.6f,.6f});
    f("uRoughness",water_.roughness);f("uReflection",water_.reflection);f("uDetail",water_.detail);f("uFoam",water_.foam);
    f("uTime",static_cast<float>(time));f("uRain",water_.rain);f("uRainScale",water_.rainScale);f("uRainSpeed",water_.rainSpeed);
    i("uDebug",debugView_);i("uSkyMode",environmentMode_);i("uSkyFlip",skyVerticalFlip_);
    f("uSkyRotation",environmentRotation_*scene::kPi/180);f("uSkyIntensity",environmentIntensity_);f("uSkyExposure",environmentExposure_);
    f("uSkyContrast",environmentContrast_);f("uSkyHighlightThreshold",environmentHighlightThreshold_);
    f("uSkyHighlightBoost",environmentHighlightBoost_);f("uSkyToneMap",environmentToneMap_);
    // Reuse the environment/shadow units already bound for the world material.
    // No texture rebinding that could invalidate the main mesh draw cache.
    for(int index=0;index<6;++index){
        i(("uFaces["+std::to_string(index)+"]").c_str(),7+index);
        i(("uSources["+std::to_string(index)+"]").c_str(),skyFaceSources_[index]);
        i(("uTurns["+std::to_string(index)+"]").c_str(),skyFaceQuarterTurns_[index]);
    }
    glapi::UniformMatrix4fv(loc("uLight"),1,GL_FALSE,light.data());glapi::UniformMatrix4fv(loc("uFarLight"),1,GL_FALSE,farLight.data());
    i("uShadow",4);i("uFarShadow",14);i("uShadows",sunEnabled_&&sunShadows_&&shadowTexture_);
    i("uFarShadows",farShadowEnabled_&&farShadowTexture_);f("uShadowBias",shadowBias_);
    f("uShadowStrength",shadowIntensity_);f("uShadowDistance",shadowDistance_);f("uFarShadowDistance",farShadowDistance_);
    i("uFog",fogEnabled_);i("uHeightFog",fogHeightEnabled_);v("uFogColor",fogColor_);
    f("uFogStart",fogStart_);f("uFogHalf",fogHalfDistance_);f("uFogOpacity",fogOpacity_);
    f("uFogHeight",fogHeight_);f("uFogFalloff",fogHeightFalloff_);
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
    glapi::DrawElements(GL_TRIANGLES,waterIndexCount_,GL_UNSIGNED_INT,nullptr);++lastTotalDrawCalls_;
    glapi::BindVertexArray(0);glapi::UseProgram(meshProgram_);
    glapi::ActiveTexture(glapi::Texture0+16);glBindTexture(GL_TEXTURE_2D,previous16);
    glapi::ActiveTexture(glapi::Texture0+17);glBindTexture(GL_TEXTURE_2D,previous17);
    glapi::ActiveTexture(glapi::Texture0+18);glBindTexture(GL_TEXTURE_2D,previous18);
    glapi::ActiveTexture(glapi::Texture0+19);glBindTexture(GL_TEXTURE_2D,previous19);
    glapi::ActiveTexture(previousActive);
}
