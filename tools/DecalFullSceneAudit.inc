// No quality, time, geometry, draw ordering or material changes in timed pairs.
std::vector<std::uint8_t> baseline;
std::vector<float> baselineDepth;
int baselineDraws{};
for(int iteration=0;iteration<8;++iteration){
    const bool enabled=(iteration+iteration/4)%2;
    app.renderer.setDiscardedDecalLightingElision(enabled);
    for(int frame=0;frame<12;++frame)draw();glFinish();
    const auto start=std::chrono::steady_clock::now();
    for(int frame=0;frame<60;++frame)draw();glFinish();
    const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/60;
    std::vector<std::uint8_t> image;std::vector<float> depth;
    CHECK(app.renderer.readColorRgba(image,error));CHECK(app.renderer.readDepthRawFloat(depth,1,100000,1,100000,false,error));
    CHECK(glGetError()==GL_NO_ERROR);
    const int draws=app.renderer.renderStats(false).lastTotalDrawCalls;
    size_t changed{};int maximum{};
    if(!iteration){baseline=image;baselineDepth=depth;baselineDraws=draws;}
    else{CHECK(image.size()==baseline.size());CHECK(depth==baselineDepth);CHECK(draws==baselineDraws);for(size_t i=0;i<image.size();++i){changed+=image[i]!=baseline[i];maximum=std::max(maximum,std::abs(int(image[i])-int(baseline[i])));}}
    decalLog<<view<<','<<slot<<','<<iteration<<','<<enabled<<','<<elapsed<<','<<draws<<','<<changed<<','<<maximum<<std::endl;
    if(iteration<2||maximum>1)CHECK(app.renderer.saveColorPng(out/("decal_"+std::to_string(view)+"_"+std::to_string(slot)+"_"+std::to_string(iteration)+".png"),error));
    CHECK(maximum<=1);
}
app.renderer.setDiscardedDecalLightingElision(false);
