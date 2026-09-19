// Included in namespace render after GL helpers.
bool StageRenderer::renderVolumetricLighting(const scene::Mat4& vp,const scene::Mat4& light,const scene::Mat4& farLight,bool foreground){
    if(!volumetric_.enabled||!sunEnabled_||volumetric_.density<=0||volumetric_.intensity<=0||debugView_!=0)return false;
    if(!volumetricProgram_){if(!volumetricError_.empty())return false;volumetricProgram_=compileProgram(kEnvironmentVertex,kVolumetricLightingFragment,volumetricError_);if(!volumetricProgram_)return false;}
    const int w=std::max(1,(width_+volumetric_.resolutionDivisor-1)/volumetric_.resolutionDivisor),h=std::max(1,(height_+volumetric_.resolutionDivisor-1)/volumetric_.resolutionDivisor);
    if(!volumetricFramebuffer_)glapi::GenFramebuffers(1,&volumetricFramebuffer_);
    if(!volumetricTexture_)glGenTextures(1,&volumetricTexture_);
    if(w!=volumetricWidth_||h!=volumetricHeight_){glBindTexture(GL_TEXTURE_2D,volumetricTexture_);glTexImage2D(GL_TEXTURE_2D,0,0x8814,w,h,0,GL_RGBA,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,glapi::ClampToEdge);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,glapi::ClampToEdge);volumetricWidth_=w;volumetricHeight_=h;}
    glapi::BindFramebuffer(glapi::Framebuffer,volumetricFramebuffer_);glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::ColorAttachment0,GL_TEXTURE_2D,volumetricTexture_,0);
    if(glapi::CheckFramebufferStatus(glapi::Framebuffer)!=glapi::FramebufferComplete){glapi::BindFramebuffer(glapi::Framebuffer,framebuffer_);return false;}
    glViewport(0,0,w,h);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glapi::BindVertexArray(lineVao_);glapi::UseProgram(volumetricProgram_);
    const auto loc=[&](const char*n){return uniformLocation(volumetricProgram_,n);};
    const auto vec=[&](const char*n,scene::Vec3 v){glapi::Uniform3f(loc(n),v.x,v.y,v.z);};
    const float px=std::sqrt(vp.v[0]*vp.v[0]+vp.v[4]*vp.v[4]+vp.v[8]*vp.v[8]),py=std::sqrt(vp.v[1]*vp.v[1]+vp.v[5]*vp.v[5]+vp.v[9]*vp.v[9]);
vec("uCamera",cameraPosition_);vec("uRight",scene::normalize(scene::Vec3{vp.v[0],vp.v[4],vp.v[8]}));vec("uUp",scene::normalize(scene::Vec3{vp.v[1],vp.v[5],vp.v[9]}));vec("uForward",scene::normalize(scene::Vec3{-vp.v[3],-vp.v[7],-vp.v[11]}));vec("uSun",scene::normalize(sunDirection_));
    vec("uRadiance",{sunColor_.x*sunIntensity_*volumetric_.red,sunColor_.y*sunIntensity_*volumetric_.green,sunColor_.z*sunIntensity_*volumetric_.blue});
    glapi::Uniform2f(loc("uRange"),cameraNear_,cameraFar_);glapi::Uniform2f(loc("uInvProjection"),1.f/std::max(.001f,px),1.f/std::max(.001f,py));glapi::Uniform2f(loc("uLowSize"),float(w),float(h));
    glapi::UniformMatrix4fv(loc("uLight"),1,GL_FALSE,light.data());glapi::UniformMatrix4fv(loc("uFarLight"),1,GL_FALSE,farLight.data());
    glapi::Uniform4f(loc("uCascade"),std::min(shadowFadeStart_,shadowDistance_-1.f),shadowDistance_,farShadowStart_,farShadowDistance_);glapi::Uniform1f(loc("uFarBlend"),farShadowBlend_);glapi::Uniform1f(loc("uBias"),shadowBias_);
    glapi::Uniform1i(loc("uShadows"),sunShadows_&&shadowTexture_);glapi::Uniform1i(loc("uFarShadows"),sunShadows_&&farShadowEnabled_&&farShadowTexture_);glapi::Uniform1i(loc("uForeground"),foreground);
    glapi::Uniform1i(loc("uSteps"),volumetric_.steps);glapi::Uniform1f(loc("uDensity"),volumetric_.density);glapi::Uniform1f(loc("uIntensity"),volumetric_.intensity);glapi::Uniform1f(loc("uHeight"),volumetric_.height);glapi::Uniform1f(loc("uHeightFalloff"),volumetric_.heightFalloff);glapi::Uniform1f(loc("uAnisotropy"),volumetric_.anisotropy);glapi::Uniform1f(loc("uMaxDistance"),volumetric_.maxDistance);
    const auto tex=[&](const char*n,int unit,unsigned t){glapi::Uniform1i(loc(n),unit);glapi::ActiveTexture(glapi::Texture0+unit);glBindTexture(GL_TEXTURE_2D,t);};
    tex("uDepth",0,depthBuffer_);tex("uShadow",1,shadowTexture_);tex("uFarShadow",2,farShadowTexture_);tex("uVolume",3,0);glapi::Uniform1i(loc("uMode"),0);glapi::DrawArrays(GL_TRIANGLES,0,3);
    // Composite through a color-only FBO, so the sampled scene depth is never
    // attached to the draw framebuffer (no depth feedback dependency).
    glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::ColorAttachment0,GL_TEXTURE_2D,colorTexture_,0);glViewport(0,0,width_,height_);tex("uVolume",3,volumetricTexture_);glapi::Uniform1i(loc("uMode"),1);
    // Add only in-scattered sun energy. Existing environment fog already applies
    // surface extinction; multiplying scene transmittance again double-fogs it.
    glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE);glapi::DrawArrays(GL_TRIANGLES,0,3);glDisable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glapi::BindFramebuffer(glapi::Framebuffer,framebuffer_);glapi::ActiveTexture(glapi::Texture0);glEnable(GL_DEPTH_TEST);return true;
}
