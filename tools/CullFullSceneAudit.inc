// Fixed poses/time. First two iterations are off/off negative controls, followed
// by alternating on/off/on. Readbacks and state queries are outside timing.
for(int variant=0;variant<(view==0&&slot==0?6:1);++variant){
    auditWire=variant==1;
    auto overlays=app.actorOverlays;
    if(variant==2){overlays.outlines=true;overlays.glow=true;overlays.throughWalls=true;}
    if(variant==3)overlays.transparentWorld=true;
    app.renderer.setActorOverlays(overlays,2);
    auto water=app.water;if(variant==4)water.enabled=true;app.renderer.setWater(water,2);
    auto ao=app.hbao;if(variant==3){ao.enabled=true;ao.separateTransparent=true;}app.renderer.setHbao(ao);
    app.renderer.setViewmodelProjection(app.weaponProfile.viewmodelFovMultiplier,variant==5?!app.weaponProfile.flipViewmodel:app.weaponProfile.flipViewmodel);
    std::vector<std::uint8_t> baseline;
    std::vector<float> baselineDepth;
    std::array<int,5> baselineState{};
    int baselineDraws{};
    for(int iteration=0;iteration<5;++iteration){
        const bool enabled=iteration==2||iteration==4;
        app.renderer.setCullStateCacheEnabled(enabled);
        glEnable(GL_CULL_FACE);glCullFace(variant%2?GL_FRONT:GL_BACK);
        for(int frame=0;frame<12;++frame)draw();glFinish();
        double submit{};const auto start=std::chrono::steady_clock::now();
        for(int frame=0;frame<50;++frame){auto begin=std::chrono::steady_clock::now();draw();submit+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();}
        glFinish();const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/50;
        std::array<int,5> state{};state[0]=glIsEnabled(GL_CULL_FACE);glGetIntegerv(GL_CULL_FACE_MODE,&state[1]);glGetIntegerv(GL_FRONT_FACE,&state[2]);glGetIntegerv(GL_DEPTH_FUNC,&state[3]);glGetIntegerv(GL_DEPTH_WRITEMASK,&state[4]);
        const auto stats=app.renderer.renderStats(false);
        CHECK(glGetError()==GL_NO_ERROR);
        std::vector<std::uint8_t> image;std::vector<float> depth;
        CHECK(app.renderer.readColorRgba(image,error));CHECK(app.renderer.readDepthRawFloat(depth,1,100000,1,100000,false,error));
        std::size_t changed{};int maximum{};
        if(iteration==0){baseline=image;baselineDepth=depth;baselineState=state;baselineDraws=stats.lastTotalDrawCalls;}
        else{CHECK(image.size()==baseline.size());for(size_t i=0;i<image.size();++i){changed+=image[i]!=baseline[i];maximum=std::max(maximum,std::abs(int(image[i])-int(baseline[i])));}
            CHECK(depth==baselineDepth);CHECK(state==baselineState);CHECK(stats.lastTotalDrawCalls==baselineDraws);}
        cullLog<<view<<','<<slot<<','<<variant<<','<<iteration<<','<<enabled<<','<<elapsed<<','<<submit/50<<','<<stats.cull.queries<<','<<stats.cull.enables<<','<<stats.cull.faces<<','<<stats.lastTotalDrawCalls<<','<<changed<<','<<maximum<<std::endl;
        if(iteration==0||iteration==2||maximum>1)CHECK(app.renderer.saveColorPng(out/("cull_"+std::to_string(view)+"_"+std::to_string(slot)+"_"+std::to_string(variant)+"_"+std::to_string(iteration)+".png"),error));
        CHECK(maximum<=1);CHECK(glGetError()==GL_NO_ERROR);
    }
}
app.renderer.setCullStateCacheEnabled(false);
app.renderer.setActorOverlays(app.actorOverlays,2);app.renderer.setWater(app.water,2);app.renderer.setHbao(app.hbao);
std::cout<<"cull view "<<view<<" slot "<<slot<<" complete\n"<<std::flush;
