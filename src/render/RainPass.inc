// Resolved color-only pass: sampled depth is never attached to the draw FBO.
void StageRenderer::renderRain(const scene::Mat4& vp){
    if(!rain_.enabled||rain_.density<=0||rain_.opacity<=0||debugView_!=0||viewmodelCapture_)return;
    if(!rainProgram_){if(!rainError_.empty())return;rainProgram_=compileProgram(kRainVertex,kRainFragment,rainError_);if(!rainProgram_)return;}
    GLint active=0,tex0=0,tex1=0,tex2=0,tex3=0;glGetIntegerv(0x84E0,&active);
    glapi::ActiveTexture(glapi::Texture0+3);glGetIntegerv(GL_TEXTURE_BINDING_2D,&tex3);
    const bool refractive=rain_.style.refractive&&rain_.style.refractionMix!=0;
    if(refractive){
        if(!rainBackground_)glGenTextures(1,&rainBackground_);
        glBindTexture(GL_TEXTURE_2D,rainBackground_);
        if(rainBackgroundWidth_!=width_||rainBackgroundHeight_!=height_){
            glTexImage2D(GL_TEXTURE_2D,0,glapi::Rgba16f,width_,height_,0,GL_RGBA,GL_FLOAT,nullptr);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,glapi::ClampToEdge);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,glapi::ClampToEdge);
            rainBackgroundWidth_=width_;rainBackgroundHeight_=height_;
        }
        glapi::BindFramebuffer(glapi::ReadFramebuffer,framebuffer_);
        glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,width_,height_);
    }
    glapi::ActiveTexture(glapi::Texture0+2);glGetIntegerv(GL_TEXTURE_BINDING_2D,&tex2);
    glapi::ActiveTexture(glapi::Texture0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&tex0);
    glapi::ActiveTexture(glapi::Texture0+1);glGetIntegerv(GL_TEXTURE_BINDING_2D,&tex1);
    if(rainField_.update(rain_,cameraPosition_,rainMap_)||!rainLaneTexture_){
        const int rows=int((rainField_.lanes.size()+255)/256)*2;
        std::vector<scene::Vec4> texels(256*rows);
        for(std::size_t k=0;k<rainField_.lanes.size();++k){const auto dst=(k/256)*512+k%256;texels[dst]=rainField_.lanes[k].base;texels[dst+256]=rainField_.lanes[k].normal;}
        if(!rainLaneTexture_)glGenTextures(1,&rainLaneTexture_);
        glBindTexture(GL_TEXTURE_2D,rainLaneTexture_);glTexImage2D(GL_TEXTURE_2D,0,glapi::Rgba32f,256,rows,0,GL_RGBA,GL_FLOAT,texels.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    }
    if(!rainFramebuffer_)glapi::GenFramebuffers(1,&rainFramebuffer_);
    glapi::BindFramebuffer(glapi::Framebuffer,rainFramebuffer_);glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::ColorAttachment0,GL_TEXTURE_2D,colorTexture_,0);
    glapi::UseProgram(rainProgram_);glapi::BindVertexArray(lineVao_);
    const auto loc=[&](const char* n){return uniformLocation(rainProgram_,n);};
    const auto f=[&](const char*n,float x){glapi::Uniform1f(loc(n),x);};
    const auto i=[&](const char*n,int x){glapi::Uniform1i(loc(n),x);};
    const auto v=[&](const char*n,scene::Vec3 x){glapi::Uniform3f(loc(n),x.x,x.y,x.z);};
    glapi::UniformMatrix4fv(loc("uVP"),1,GL_FALSE,vp.data());glapi::Uniform2f(loc("uViewport"),float(width_),float(height_));glapi::Uniform2f(loc("uRange"),cameraNear_,cameraFar_);
    v("uCamera",cameraPosition_);v("uSlope",rain::slope(rain_));v("uTint",rain_.tint);v("uSun",scene::normalize(sunDirection_));
    v("uLight",sunEnabled_?ambientColor_*sunAmbient_+sunColor_*(sunIntensity_*.35f):scene::Vec3{1,1,1});
    f("uPhase",float(rain::phase(rainTime_,rain_)));f("uSpeed",rain::safeSpeed(rain_)*100);f("uRadius",rain::safeRadius(rain_)*100);
    i("uHasShelter",rainShelterReady_);v("uShelterTile",{rainShelterCenter_.x,rainShelterCenter_.y,rainShelterExtent_});
    f("uCellSize",rain::safeRadius(rain_)*100/(rain_.quality==0?16:rain_.quality==1?24:32));
    const double time=rain::clock(rainTime_,rain_);
    f("uEventTime",float(std::remainder(time,65536.)));
    // Individual phase reduction retains precision over long takes without a
    // shared wrap discontinuity between the differently paced noise bands.
    const auto angle=[&](double rate){return float(std::remainder(time*rate,2.*scene::kPi));};
    v("uNoiseTime",{angle(.61*rain_.turbulenceRate),angle(-.83*rain_.turbulenceRate),angle(.47*rain_.turbulenceRate)});
    const float rate=2*scene::kPi/std::max(.001f,std::abs(rain_.gustPeriod));
    glapi::Uniform2f(loc("uGustTime"),angle(rate),angle(rate*.713));
    f("uGustRate",rate);f("uGust",rain_.gusts*std::sqrt(std::abs(rain_.wind))*25);
    f("uTurbulence",rain_.turbulence*35);f("uSwirlSize",rain_.turbulenceScale*100);f("uSwirlRate",rain_.turbulenceRate);
    i("uWallMist",rain_.wallMist);f("uMistAmount",rain_.mistAmount);f("uMistSize",rain_.mistSize);
    f("uMistTime",float(std::fmod(time*.65,1.)));
    f("uWidth",std::max(.0001f,rain_.width));f("uShutter",std::max(.00001f,rain_.shutter));f("uDensity",rain_.density);f("uOpacity",rain_.opacity);
    f("uBrightness",rain_.brightness);f("uNearFade",std::max(.001f,rain_.nearFade));f("uSoftness",std::max(.001f,rain_.softness));
    f("uSplashSize",rain_.splashSize);f("uSplashAmount",rain_.splashAmount);i("uSplashes",rain_.splashes);
    const auto& style=rain_.style;
    glapi::Uniform4f(loc("uSplashStyle"),style.rate,style.lifetime,style.jitter,style.fade);
    glapi::Uniform4f(loc("uSplashShape"),style.expansion,style.crownHeight,style.crownSpread,0);
    glapi::Uniform4f(loc("uSplashDetail"),style.ringOpacity,style.ringWidth,style.dropSize,style.dropCount);
    v("uAerosol",{style.grainScale,style.grainAmount,style.grainContrast});
    glapi::Uniform4f(loc("uRefraction"),style.refractionMix,style.ior,style.roughness,style.refractionStrength);
    i("uRefractive",refractive);i("uRainBackground",3);
    glapi::ActiveTexture(glapi::Texture0+3);glBindTexture(GL_TEXTURE_2D,rainBackground_);
    i("uForeground",foregroundDrawn_&&firstPersonProjection_);i("uFog",fogEnabled_);v("uFogColor",fogColor_);
    f("uFogStart",fogStart_);f("uFogHalf",fogHalfDistance_);f("uFogOpacity",fogOpacity_);
    i("uHeightFog",fogHeightEnabled_);f("uFogHeight",fogHeight_);f("uFogFalloff",fogHeightFalloff_);
    glapi::ActiveTexture(glapi::Texture0);glBindTexture(GL_TEXTURE_2D,depthBuffer_);i("uDepth",0);
    glapi::ActiveTexture(glapi::Texture0+1);glBindTexture(GL_TEXTURE_2D,rainLaneTexture_);i("uLanes",1);
    glapi::ActiveTexture(glapi::Texture0+2);glBindTexture(GL_TEXTURE_2D,rainShelterTexture_);i("uShelter",2);
    glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDepthMask(GL_FALSE);glEnable(GL_BLEND);
    // Preserve destination alpha (isolated world/capture exports included).
    glapi::BlendFuncSeparate(GL_ONE,GL_ONE_MINUS_SRC_ALPHA,GL_ZERO,GL_ONE);
    glapi::DrawArrays(GL_TRIANGLES,0,int(rainField_.lanes.size())*16*6);++lastTotalDrawCalls_;
    glDisable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_TRUE);glEnable(GL_DEPTH_TEST);
    glapi::BindFramebuffer(glapi::Framebuffer,framebuffer_);glapi::BindVertexArray(0);
    glapi::ActiveTexture(glapi::Texture0);glBindTexture(GL_TEXTURE_2D,tex0);
    glapi::ActiveTexture(glapi::Texture0+1);glBindTexture(GL_TEXTURE_2D,tex1);
    glapi::ActiveTexture(glapi::Texture0+2);glBindTexture(GL_TEXTURE_2D,tex2);
    glapi::ActiveTexture(glapi::Texture0+3);glBindTexture(GL_TEXTURE_2D,tex3);glapi::ActiveTexture(active);
}
