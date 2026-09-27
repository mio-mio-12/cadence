// Wind-aligned local surface atlas. Opaque/alpha-tested map geometry, including
// off-camera roofs, is rendered once per snapped tile instead of thousands of
// synchronous collision queries. No CPU readback or simulation history.
void StageRenderer::prepareRainShelter(bool weatherMode){
    // Separate caches: snowfall must never invalidate or redirect rain, wetness,
    // sunlight shadows or gameplay collision. Share only the atlas algorithm.
    auto& rainShelterReady_=weatherMode?weatherShelter_.ready:this->rainShelterReady_;
    auto& rainShelterValid_=weatherMode?weatherShelter_.valid:this->rainShelterValid_;
    auto& rainShelterFramebuffer_=weatherMode?weatherShelter_.framebuffer:this->rainShelterFramebuffer_;
    auto& rainShelterTexture_=weatherMode?weatherShelter_.texture:this->rainShelterTexture_;
    auto& rainShelterDepth_=weatherMode?weatherShelter_.depth:this->rainShelterDepth_;
    auto& rainShelterCenter_=weatherMode?weatherShelter_.center:this->rainShelterCenter_;
    auto& rainShelterSlope_=weatherMode?weatherShelter_.slope:this->rainShelterSlope_;
    auto& rainShelterExtent_=weatherMode?weatherShelter_.extent:this->rainShelterExtent_;
    auto& rainShelterCeiling_=weatherMode?weatherShelter_.ceiling:this->rainShelterCeiling_;
    auto& ceilingReuse=weatherMode?weatherShelter_.ceilingReuse:rainShelterCeilingReuse_;
    auto& candidates=weatherMode?weatherShelter_.candidates:rainShelterCandidates_;
    auto& rainError_=weatherMode?weatherError_:this->rainError_;
    rain::Settings settings=this->rain_;
    if(weatherMode){settings.enabled=weather_.sheltered();settings.shelter=settings.enabled;settings.wind=0;settings.radius=weather::safe(weather_.range,100,20000)/100;settings.blockerDistance=weather::safe(weather_.blockerDistance,0,1000000)/100;}
    const auto& rain_=settings;
    rainShelterReady_=false;
    if(!rain_.shelter||(!rain_.enabled&&!wet_.ground&&!wet_.enabled)||!mapMeshCount_)return;
    const auto drift=rain::slope(rain_);
    const float radius=rain::safeRadius(rain_)*100;
    const float extent=radius*1.75f;
    const float ceiling=std::isfinite(rain_.blockerDistance)&&rain_.blockerDistance>0
        ?std::ceil(cameraPosition_.z/100.f)*100.f+rain_.blockerDistance*100.f/std::sqrt(1.f+drift.x*drift.x+drift.y*drift.y):1e8f;
    // Whole-texel translations keep overlapping shelter samples in identical
    // world locations when the cache moves; fractional shifts shimmer/pop.
    const int resolution=weatherMode?1024:2048;
    const float texel=2.f*extent/float(resolution);
    const float step=std::max(1.f,std::floor(radius*.5f/texel))*texel;
    const scene::Vec3 center{std::floor((cameraPosition_.x+drift.x*cameraPosition_.z)/step)*step,
        std::floor((cameraPosition_.y+drift.y*cameraPosition_.z)/step)*step,0};
    const bool sameTile=rainShelterValid_&&center.x==rainShelterCenter_.x&&center.y==rainShelterCenter_.y&&extent==rainShelterExtent_
        &&drift.x==rainShelterSlope_.x&&drift.y==rainShelterSlope_.y;
    if(sameTile&&(ceiling==rainShelterCeiling_||ceilingReuse.contains(ceiling))){rainShelterReady_=true;return;}
    if(!rainShelterProgram_){rainShelterProgram_=compileProgram(kRainShelterVertex,kRainShelterFragment,rainError_);if(!rainShelterProgram_)return;}
    GLint fb{},viewport[4]{},active{},tex{};GLfloat clear[4]{};GLboolean depthMask{};
    glGetIntegerv(0x8CA6,&fb);glGetIntegerv(GL_VIEWPORT,viewport);glGetFloatv(GL_COLOR_CLEAR_VALUE,clear);
    glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);const bool depth=glIsEnabled(GL_DEPTH_TEST),blend=glIsEnabled(GL_BLEND),cull=glIsEnabled(GL_CULL_FACE);
    glGetIntegerv(0x84E0,&active);glapi::ActiveTexture(glapi::Texture0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&tex);
    if(!rainShelterFramebuffer_){
        glapi::GenFramebuffers(1,&rainShelterFramebuffer_);glGenTextures(1,&rainShelterTexture_);glGenTextures(1,&rainShelterDepth_);
        glBindTexture(GL_TEXTURE_2D,rainShelterTexture_);glTexImage2D(GL_TEXTURE_2D,0,glapi::Rgba32f,resolution,resolution,0,GL_RGBA,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,glapi::ClampToEdge);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,glapi::ClampToEdge);
        glBindTexture(GL_TEXTURE_2D,rainShelterDepth_);glTexImage2D(GL_TEXTURE_2D,0,0x8CAC,resolution,resolution,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    }
    float top=-1e8f,bottom=1e8f;
    float nextExcludedBottom=1e30f;
    std::vector<std::size_t> visible;
    // A height-only change cannot change wind-space XY overlap. Keep all tile
    // candidates, including those above the old ceiling, for exact reuse.
    if(!sameTile){
    candidates.clear();
    for(std::size_t i=mapMeshFirst_;i<mapMeshFirst_+mapMeshCount_&&i<meshes_.size();++i){
        const auto& m=meshes_[i];
        float minX=1e30f,minY=1e30f,maxX=-1e30f,maxY=-1e30f,lo=1e30f,hi=-1e30f;
        for(int k=0;k<8;++k){const auto p=scene::transformPoint(m.model,{k&1?m.aabbMax.x:m.aabbMin.x,k&2?m.aabbMax.y:m.aabbMin.y,k&4?m.aabbMax.z:m.aabbMin.z});
            const float x=p.x+drift.x*p.z,y=p.y+drift.y*p.z;
            minX=std::min(minX,x);maxX=std::max(maxX,x);minY=std::min(minY,y);maxY=std::max(maxY,y);lo=std::min(lo,p.z);hi=std::max(hi,p.z);}
        if(maxX<center.x-extent||minX>center.x+extent||maxY<center.y-extent||minY>center.y+extent)continue;
        candidates.push_back({i,lo,hi});
    }
    }
    for(const auto& candidate:candidates){
        const auto i=candidate.index;const auto& m=meshes_[i];
        if(m.rainExcluded||m.decal||m.decalAdditive||m.decalMultiply||m.lens||(m.forceAlpha&&!m.alphaTest))continue;
        const float lo=candidate.bottom,hi=candidate.top;
        if(lo>ceiling){nextExcludedBottom=std::min(nextExcludedBottom,lo);continue;}
        top=std::max(top,hi);bottom=std::min(bottom,lo);visible.push_back(i);
    }
    ceilingReuse.set(top,nextExcludedBottom,top>bottom,ceiling);
    if(top<=bottom){top=cameraPosition_.z+1000;bottom=cameraPosition_.z-1000;}
    top+=10;bottom-=10;
    top=std::min(top,ceiling);if(bottom>=top)bottom=top-1000;
    glapi::BindFramebuffer(glapi::Framebuffer,rainShelterFramebuffer_);
    glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::ColorAttachment0,GL_TEXTURE_2D,rainShelterTexture_,0);
    glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::DepthAttachment,GL_TEXTURE_2D,rainShelterDepth_,0);
    const bool complete=glapi::CheckFramebufferStatus(glapi::Framebuffer)==glapi::FramebufferComplete;
    if(complete){
        glViewport(0,0,resolution,resolution);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);
        glClearColor(-1e8f,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glapi::UseProgram(rainShelterProgram_);
        const auto loc=[&](const char* n){return uniformLocation(rainShelterProgram_,n);};
        glapi::Uniform4f(loc("uTile"),center.x,center.y,extent,0);glapi::Uniform4f(loc("uWindRange"),drift.x,drift.y,bottom,top);
        glapi::Uniform1i(loc("uAlbedo"),0);
        for(const auto i:visible){const auto& m=meshes_[i];glapi::UniformMatrix4fv(loc("uModel"),1,GL_FALSE,m.model.data());
            glapi::Uniform1f(loc("uCutoff"),m.alphaTest&&!m.ignoreAlbedoAlpha&&m.texture?m.alphaCutoff:-1.f);
            glBindTexture(GL_TEXTURE_2D,m.texture);glapi::BindVertexArray(m.vao);glDrawElements(GL_TRIANGLES,m.indexCount,GL_UNSIGNED_INT,nullptr);}
    }else rainError_="Rain shelter framebuffer is incomplete";
    glapi::BindVertexArray(0);glapi::UseProgram(0);glapi::BindFramebuffer(glapi::Framebuffer,fb);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
    glClearColor(clear[0],clear[1],clear[2],clear[3]);glDepthMask(depthMask);
    if(!depth)glDisable(GL_DEPTH_TEST);if(blend)glEnable(GL_BLEND);if(cull)glEnable(GL_CULL_FACE);
    glBindTexture(GL_TEXTURE_2D,tex);glapi::ActiveTexture(active);
    rainShelterCenter_=center;rainShelterSlope_=drift;rainShelterExtent_=extent;rainShelterCeiling_=ceiling;rainShelterValid_=rainShelterReady_=complete;
}
