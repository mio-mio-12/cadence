// Included inside namespace render after GL helpers are available.
void StageRenderer::configureAoFog(unsigned program,const scene::Mat4& vp,bool foreground){
    const auto loc=[&](const char* name){return uniformLocation(program,name);};
    glapi::Uniform1i(loc("uAoBeforeFog"),hbao_.enabled&&hbao_.beforeFog&&fogEnabled_);
    if(!hbao_.enabled||!hbao_.beforeFog||!fogEnabled_)return;
    const float px=std::sqrt(vp.v[0]*vp.v[0]+vp.v[4]*vp.v[4]+vp.v[8]*vp.v[8]);
    const float py=std::sqrt(vp.v[1]*vp.v[1]+vp.v[5]*vp.v[5]+vp.v[9]*vp.v[9]);
    glapi::Uniform2f(loc("uAoFogInvProjection"),1.f/std::max(.001f,px),1.f/std::max(.001f,py));
    // Rows of projection*view recover right/up/forward world-Z components.
    glapi::Uniform3f(loc("uAoFogWorldZ"),vp.v[8]/std::max(.001f,px),vp.v[9]/std::max(.001f,py),vp.v[11]);
    glapi::Uniform1f(loc("uAoFogCameraZ"),cameraPosition_.z);
    glapi::Uniform2f(loc("uAoFogDepthRange"),cameraNear_,cameraFar_);
    glapi::Uniform1i(loc("uAoFogForeground"),foreground);
    glapi::Uniform3f(loc("uAoFogColor"),fogColor_.x,fogColor_.y,fogColor_.z);
    glapi::Uniform1f(loc("uAoFogStart"),fogStart_);glapi::Uniform1f(loc("uAoFogHalfDistance"),fogHalfDistance_);
    glapi::Uniform1f(loc("uAoFogOpacity"),fogOpacity_);glapi::Uniform1i(loc("uAoFogHeightEnabled"),fogHeightEnabled_);
    glapi::Uniform1f(loc("uAoFogHeight"),fogHeight_);glapi::Uniform1f(loc("uAoFogHeightFalloff"),fogHeightFalloff_);
    glapi::Uniform1i(loc("uAoFogDepth"),8);glapi::ActiveTexture(glapi::Texture0+8);glBindTexture(GL_TEXTURE_2D,depthBuffer_);
    glapi::ActiveTexture(glapi::Texture0);
}

bool StageRenderer::renderHbao(const scene::Mat4& vp,bool foreground){
    if(!hbaoProgram_)return false;
    if(!hbaoFramebuffer_){
        glapi::GenFramebuffers(1,&hbaoFramebuffer_);glGenTextures(2,hbaoTextures_.data());
        for(const auto texture:hbaoTextures_){glBindTexture(GL_TEXTURE_2D,texture);
            // RG32F stores visibility and linear depth without half-float overflow.
            glTexImage2D(GL_TEXTURE_2D,0,0x8230,width_,height_,0,0x8227,GL_FLOAT,nullptr);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,glapi::ClampToEdge);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,glapi::ClampToEdge);
        }
    }
    glapi::BindFramebuffer(glapi::Framebuffer,hbaoFramebuffer_);glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::ColorAttachment0,GL_TEXTURE_2D,hbaoTextures_[0],0);
    if(glapi::CheckFramebufferStatus(glapi::Framebuffer)!=glapi::FramebufferComplete){glapi::BindFramebuffer(glapi::Framebuffer,framebuffer_);return false;}
    glViewport(0,0,width_,height_);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glapi::BindVertexArray(lineVao_);glapi::UseProgram(hbaoProgram_);
    const auto loc=[&](const char* name){return uniformLocation(hbaoProgram_,name);};
    glapi::Uniform1i(loc("uForeground"),foreground?1:0);glapi::Uniform1i(loc("uWeaponBackgroundHalo"),hbao_.weaponBackgroundHalo?1:0);
    glapi::Uniform1i(loc("uDepth"),0);glapi::ActiveTexture(glapi::Texture0);glBindTexture(GL_TEXTURE_2D,depthBuffer_);
    glapi::Uniform1i(loc("uAo"),1);glapi::Uniform2f(loc("uTexel"),1.f/width_,1.f/height_);
    const float px=std::sqrt(vp.v[0]*vp.v[0]+vp.v[4]*vp.v[4]+vp.v[8]*vp.v[8]),py=std::sqrt(vp.v[1]*vp.v[1]+vp.v[5]*vp.v[5]+vp.v[9]*vp.v[9]);
    glapi::Uniform2f(loc("uInvProjection"),1.f/std::max(.001f,px),1.f/std::max(.001f,py));
    glapi::Uniform1f(loc("uNear"),cameraNear_);glapi::Uniform1f(loc("uFar"),cameraFar_);
    glapi::Uniform1f(loc("uRadius"),hbao_.radius);glapi::Uniform1f(loc("uBias"),hbao_.biasDegrees*scene::kPi/180.f);glapi::Uniform1f(loc("uFalloff"),hbao_.falloff);
    glapi::Uniform1f(loc("uMaxPixels"),hbao_.maxPixels);glapi::Uniform1f(loc("uSharpness"),hbao_.blurSharpness);
    glapi::Uniform1i(loc("uDirections"),hbao_.directions);glapi::Uniform1i(loc("uSteps"),hbao_.steps);glapi::Uniform1i(loc("uBlurRadius"),hbao_.blurRadius);
    glapi::Uniform1i(loc("uMode"),0);glapi::ActiveTexture(glapi::Texture0+1);glBindTexture(GL_TEXTURE_2D,0);glapi::DrawArrays(GL_TRIANGLES,0,3);
    if(hbao_.blurRadius>0){glapi::Uniform1i(loc("uMode"),1);
        for(int pass=0;pass<2;++pass){glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::ColorAttachment0,GL_TEXTURE_2D,hbaoTextures_[1-pass],0);
            glapi::ActiveTexture(glapi::Texture0+1);glBindTexture(GL_TEXTURE_2D,hbaoTextures_[pass]);glapi::Uniform2f(loc("uBlurAxis"),pass==0?1.f:0.f,pass==1?1.f:0.f);glapi::DrawArrays(GL_TRIANGLES,0,3);}
    }
    glapi::ActiveTexture(glapi::Texture0);glapi::BindFramebuffer(glapi::Framebuffer,framebuffer_);glEnable(GL_DEPTH_TEST);return true;
}
