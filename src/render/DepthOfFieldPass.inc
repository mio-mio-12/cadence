bool StageRenderer::renderDepthOfField(bool foreground,bool ao,bool isolation){
    if(!dofProgram_)return false;
    const int w=std::max(1,width_/dof_.downsample),h=std::max(1,height_/dof_.downsample);
    if(!dofFramebuffer_)glapi::GenFramebuffers(1,&dofFramebuffer_);
    if(w!=dofWidth_||h!=dofHeight_){
        for(auto& t:dofTextures_)if(t){glDeleteTextures(1,&t);t=0;}glGenTextures(2,dofTextures_.data());
        for(auto t:dofTextures_){glBindTexture(GL_TEXTURE_2D,t);glTexImage2D(GL_TEXTURE_2D,0,glapi::Rgba16f,w,h,0,GL_RGBA,GL_FLOAT,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,glapi::ClampToEdge);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,glapi::ClampToEdge);}dofWidth_=w;dofHeight_=h;
    }
    glapi::BindFramebuffer(glapi::Framebuffer,dofFramebuffer_);glViewport(0,0,w,h);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glapi::BindVertexArray(lineVao_);glapi::UseProgram(dofProgram_);
    const auto loc=[&](const char* n){return uniformLocation(dofProgram_,n);};const auto f=[&](const char* n,float v){glapi::Uniform1f(loc(n),v);};const auto i=[&](const char* n,int v){glapi::Uniform1i(loc(n),v);};
    for(int unit=0;unit<4;++unit){glapi::ActiveTexture(glapi::Texture0+unit);glBindTexture(GL_TEXTURE_2D,unit==0?colorTexture_:unit==1?depthBuffer_:unit==2?hbaoTextures_[0]:aoIsolationTexture_);}
    i("uSource",0);i("uDepth",1);i("uAo",2);i("uIsolated",3);i("uForeground",foreground);i("uAffectViewmodel",dof_.affectViewmodel);i("uAoEnabled",ao);i("uIsolation",isolation);
    glapi::Uniform2f(loc("uTexel"),1.f/width_,1.f/height_);f("uNear",cameraNear_);f("uFar",cameraFar_);f("uFocus",dof_.focusDistance);f("uRange",dof_.focusRange);f("uNearTransition",dof_.nearTransition);f("uFarTransition",dof_.farTransition);f("uNearRadius",dof_.nearRadius);f("uFarRadius",dof_.farRadius);f("uGamma",dof_.gamma);f("uBokeh",dof_.bokehAmount);f("uThreshold",dof_.bokehThreshold);f("uHollow",dof_.hollowness);f("uAnamorphic",dof_.anamorphic);f("uRotation",dof_.rotationDegrees*scene::kPi/180);f("uAoIntensity",hbao_.intensity);f("uAoPower",hbao_.power);i("uSamples",dof_.samples);i("uBlades",dof_.blades);
    for(int pass=0;pass<2;++pass){glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::ColorAttachment0,GL_TEXTURE_2D,dofTextures_[pass],0);if(glapi::CheckFramebufferStatus(glapi::Framebuffer)!=glapi::FramebufferComplete){glapi::BindFramebuffer(glapi::Framebuffer,framebuffer_);glEnable(GL_DEPTH_TEST);return false;}i("uMode",pass);glapi::DrawArrays(GL_TRIANGLES,0,3);}
    glapi::BindFramebuffer(glapi::Framebuffer,framebuffer_);glViewport(0,0,width_,height_);glapi::ActiveTexture(glapi::Texture0);glEnable(GL_DEPTH_TEST);return true;
}
