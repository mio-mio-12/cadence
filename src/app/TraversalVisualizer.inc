if(app.traversalVisualizer&&app.actorMode&&!app.takePreview&&!app.exportActive){
    const auto& settings=app.traversalSettings;
    const float eye=std::max(gameplay::iw::worldUnits(20),app.actorViewHeight),radius=app.movementAlgorithm==3?gameplay::source_v2::units(16):scene::course::kPlayerRadius;
    const float minimum=eye*settings.minimumEyePercent*.01f,maximum=eye*settings.maximumEyePercent*.01f,reach=radius+gameplay::iw::worldUnits(settings.reachIw);
    const scene::Vec3 forward{std::cos(app.actorYaw),std::sin(app.actorYaw),0},side{-forward.y,forward.x,0};
    auto& debug=app.traversalDebug;
    // Read-only height/reach probe, throttled and only while visualizing.
    if(app.loadedMap&&!app.actorMantling&&app.gameplayClock-debug.time>(debug.attempted?1.f:.1f)){
        debug={app.actorPosition,app.actorPosition+forward*reach,gameplay::traversal::Decision::NoFace,app.gameplayClock,0,false};
        for(int i=1;i<=4;++i){
            auto candidate=app.actorPosition+forward*(radius+(reach)*i*.25f);
            const float top=app.loadedMap->groundHeight(candidate.x,candidate.y,app.actorPosition.z+std::max(maximum*1.3f,eye)-45.f,-1e9f);
            if(top<=app.actorPosition.z+1.f)continue;
            candidate.z=top;debug.target=candidate;
            debug.decision=gameplay::traversal::classify(*app.loadedMap,app.actorPosition,candidate,forward,eye,radius,settings);
            break;
        }
    }
    feedbackDraw->PushClipRect(viewportImageMin,viewportImageMax,true);
    const auto line=[&](scene::Vec3 a,scene::Vec3 b,ImU32 color){const auto x=projectPoint(a),y=projectPoint(b);if(x&&y)feedbackDraw->AddLine(*x,*y,color,2);};
    const auto center=app.actorPosition+forward*reach;
    const ImU32 green=IM_COL32(95,240,130,230),cyan=IM_COL32(80,205,255,230),orange=IM_COL32(255,185,65,230),red=IM_COL32(255,95,85,230);
    for(const auto h:{minimum,maximum}){
        const auto a=center-side*radius+scene::Vec3{0,0,h},b=center+side*radius+scene::Vec3{0,0,h};
        line(a,b,h==minimum?green:cyan);
    }
    for(float sign:{-1.f,1.f})line(center+side*(sign*radius)+scene::Vec3{0,0,minimum},center+side*(sign*radius)+scene::Vec3{0,0,maximum},green);
    if(settings.airEdgeEnabled)line(center-side*radius+scene::Vec3{0,0,gameplay::iw::worldUnits(settings.airEdgeIw)},center+side*radius+scene::Vec3{0,0,gameplay::iw::worldUnits(settings.airEdgeIw)},orange);
    if(app.gameplayClock-debug.time<2){
        const bool allowed=debug.decision==gameplay::traversal::Decision::Eligible||debug.decision==gameplay::traversal::Decision::Started;
        if(const auto p=projectPoint(debug.target)){feedbackDraw->AddCircle(*p,7,allowed?green:red,16,2);feedbackDraw->AddText({p->x+10,p->y},allowed?green:red,gameplay::traversal::label(debug.decision));}
    }
    char text[512]{};
    std::snprintf(text,sizeof(text),"TRAVERSAL\nGrab %.1f - %.1f IW | Reach %.1f IW\n%s: %s\nCandidate rise %.1f IW | Speed %.1f IW/s\nEdge assist %s | Used %.2f / %.2f IW",minimum/gameplay::iw::worldUnits(1),maximum/gameplay::iw::worldUnits(1),settings.reachIw,debug.attempted?"Last attempt":"Height/reach probe (path untested)",gameplay::traversal::label(debug.decision),(debug.target.z-debug.start.z)/gameplay::iw::worldUnits(1),std::hypot(app.actorVelocity.x,app.actorVelocity.y)/gameplay::iw::worldUnits(1),settings.airEdgeEnabled?"on":"off",app.actorAirEdgeLift/gameplay::iw::worldUnits(1),settings.airEdgeIw);
    const auto size=ImGui::CalcTextSize(text);const ImVec2 at{viewportImageMin.x+12,viewportImageMin.y+12};
    feedbackDraw->AddRectFilled({at.x-5,at.y-4},{at.x+size.x+5,at.y+size.y+4},IM_COL32(10,14,20,220),4);
    feedbackDraw->AddText(at,cyan,text);
    feedbackDraw->PopClipRect();
}
