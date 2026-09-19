// Re-render only blended map contributions into a transparent buffer. The
// shader rejects fragments close to their underlying surface, preserving AO
// on attached decals irrespective of camera distance. Final composition is
// C * AO + transparentPremultiplied * (1 - AO), not an alpha/AO exemption mask.
bool aoIsolationActive=false;
if(hbao_.enabled&&hbao_.separateTransparent&&(debugView_==0||debugView_==1)&&mapScene&&!viewmodelCapture_&&!wireframe&&(!mapTransparentMeshes_.empty()||!mapDecalMeshes_.empty()||!mapAdditiveMeshes_.empty())){
    if(!aoIsolationFramebuffer_){glapi::GenFramebuffers(1,&aoIsolationFramebuffer_);glGenTextures(1,&aoIsolationTexture_);glBindTexture(GL_TEXTURE_2D,aoIsolationTexture_);glTexImage2D(GL_TEXTURE_2D,0,glapi::Rgba16f,width_,height_,0,GL_RGBA,GL_FLOAT,nullptr);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,glapi::ClampToEdge);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,glapi::ClampToEdge);}
    glapi::BindFramebuffer(glapi::Framebuffer,aoIsolationFramebuffer_);glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::ColorAttachment0,GL_TEXTURE_2D,aoIsolationTexture_,0);
    if(glapi::CheckFramebufferStatus(glapi::Framebuffer)==glapi::FramebufferComplete){
        glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);glDisable(GL_DEPTH_TEST);glapi::UseProgram(meshProgram_);glDisable(GL_CULL_FACE);glDepthRange(0,1);
        glapi::Uniform1i(uniformLocation(meshProgram_,"uAoIsolation"),1);glapi::Uniform1i(uniformLocation(meshProgram_,"uAoHasForeground"),showMainScene);glapi::Uniform1i(uniformLocation(meshProgram_,"uAoOpaqueDepth"),15);glapi::Uniform2f(uniformLocation(meshProgram_,"uAoDepthRange"),cameraNear_,cameraFar_);glapi::Uniform1f(uniformLocation(meshProgram_,"uAoGap"),std::clamp(hbao_.transparentGap,.1f,100.f));glapi::ActiveTexture(glapi::Texture0+15);glBindTexture(GL_TEXTURE_2D,depthBuffer_);
        glapi::UniformMatrix4fv(meshUniforms_.model,1,GL_FALSE,scene::Mat4::identity().data());glapi::Uniform1i(meshUniforms_.ignoreAlpha,ignoreMapTextureAlpha_);glapi::Uniform1i(meshUniforms_.viewmodelSurface,0);glapi::Uniform1i(meshUniforms_.skinned,0);glapi::Uniform1i(meshUniforms_.camoBlend,0);glapi::Uniform1i(meshUniforms_.camoLumaMask,0);glapi::Uniform1i(meshUniforms_.camoUseAlpha,0);glapi::Uniform1i(meshUniforms_.lens,0);
        // This pass resets raw uniforms/bindings outside drawMesh.
        materialState=MeshUniformCache{};
        lastTex13=lastTex0=lastTex1=lastTex3=lastTex5=lastTex6=0xFFFFFFFF;lastVao=0;lastSurfaceProfile=-999;
        const auto frustum=scene::extractFrustum(viewProjection);const auto pass=[&](const auto& indices){for(auto idx:indices){const auto& m=meshes_[idx];if(m.decalMultiply||m.alphaTest||!(m.forceAlpha||m.decal||m.decalAdditive))continue;if(m.hasBounds&&!scene::isAabbInFrustum(frustum,m.aabbMin,m.aabbMax))continue;drawMesh(m,*mapScene,{});}};
        pass(mapDecalMeshes_);pass(mapTransparentMeshes_);pass(mapAdditiveMeshes_);
        glapi::Uniform1i(uniformLocation(meshProgram_,"uAoIsolation"),0);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDisable(GL_BLEND);aoIsolationActive=true;
    }
    glapi::BindFramebuffer(glapi::Framebuffer,framebuffer_);glapi::ActiveTexture(glapi::Texture0);
}
