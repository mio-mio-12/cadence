void drawCameraMotionControls(AppState& app){
    if(ImGui::TreeNode("Shoulder camera")){
        auto& s=app.cameraControls.shoulder;
        ImGui::Checkbox("Over-shoulder third person",&app.actorThirdPerson);
        ImGui::SliderFloat3("Hip position forward / right / up",&s.position.x,-300,100,"%.1f IW units");
        ImGui::SliderFloat3("ADS position forward / right / up",&s.adsPosition.x,-300,100,"%.1f IW units");
        ImGui::SliderFloat3("Orbit pitch / yaw / camera roll",&s.rotation.x,-90,90,"%.1f deg");
        ImGui::SliderFloat("Hip zoom (FOV)",&s.fov,5,150,"%.1f deg");
        ImGui::SliderFloat("ADS zoom (FOV)",&s.adsFov,5,150,"%.1f deg");
        s.sanitize();
        uiHelp("Pitch and yaw orbit around the actor. The camera stays aimed at the firing-ray target, keeping the crosshair centered on aim. Walls shorten the camera boom.");
        if(ImGui::Button("Save defaults"))saveUserMotionDefaults(app,gameplay::view::MotionSection::Shoulder);
        ImGui::TreePop();
    }
    if(!ImGui::CollapsingHeader("Motion"))return;
    const auto save=[&](gameplay::view::MotionSection section){if(ImGui::Button("Save defaults"))saveUserMotionDefaults(app,section);};
    if(ImGui::TreeNode("Universal first-person mantle")){
    ImGui::Checkbox("Authored first-person mantle animations",&app.experimentalAuthoredViewmodelMantle);

    ImGui::Checkbox("Use procedural fallback when no authored viewmodel mantle exists",&app.experimentalUniversalFirstPersonMantle);
    ImGui::BeginDisabled(!app.experimentalUniversalFirstPersonMantle);
    ImGui::SliderFloat("Fallback intensity",&app.universalFirstPersonMantleScale,0.0f,2.0f,"%.2fx");
    ImGui::SliderFloat3("Lowered position X / Y / Z",&app.mantleFallbackPositionIw.x,-20.0f,20.0f,"%.1f IW units");
    ImGui::SliderFloat3("Tilt rotation X / Y / Z",&app.mantleFallbackRotationDegrees.x,-90.0f,90.0f,"%.1f deg");
    ImGui::SliderFloat("One-shot duration",&app.mantleFallbackRecovery,0.05f,2.0f,"%.2f sec");
    drawResponseCurve("mantle",app.mantleFallbackCurve);
    ImGui::EndDisabled();
    save(gameplay::view::MotionSection::Mantle);ImGui::TreePop();}
    for(int motion=0;motion<2;++motion){
        ImGui::PushID(motion);auto& profile=motion==0?app.boostFallback:app.slideFallback;
        if(ImGui::TreeNode(motion==0?"Boost-jump procedural fallback":"Directional slide motion")){
        if(motion==0)ImGui::Checkbox("Authored boost-jump clips",&app.experimentalExoViewmodelJump);
        ImGui::Checkbox("Enable motion",&profile.enabled);ImGui::BeginDisabled(!profile.enabled);
        ImGui::SliderFloat("Intensity",&profile.intensity,0,2,"%.2fx");
        ImGui::SliderFloat("Duration",&profile.duration,.05f,5,"%.2f sec");
        ImGui::SliderFloat3("Position X / Y / Z",&profile.position.x,-20,20,"%.1f IW units");
        ImGui::SliderFloat3("Rotation X / Y / Z",&profile.rotation.x,-90,90,"%.1f deg");
        drawResponseCurve("response",profile.curve);ImGui::EndDisabled();save(motion==0?gameplay::view::MotionSection::Boost:gameplay::view::MotionSection::Slide);ImGui::TreePop();}ImGui::PopID();
    }
    if(ImGui::TreeNode("Boost-jump camera feedback")){
    ImGui::Checkbox("Enable camera feedback",&app.boostCamera.enabled);
    ImGui::BeginDisabled(!app.boostCamera.enabled);
    ImGui::Checkbox("Move viewmodel with camera",&app.boostCameraMovesViewmodel);
    ImGui::SliderFloat("Camera intensity",&app.boostCamera.intensity,0,2,"%.2fx");
    ImGui::SliderFloat("Camera duration",&app.boostCamera.duration,.05f,5,"%.2f sec");
    ImGui::SliderFloat3("Camera position forward / left / up",&app.boostCamera.position.x,-20,20,"%.1f IW units");
    ImGui::SliderFloat3("Camera roll / pitch / yaw",&app.boostCamera.rotation.x,-45,45,"%.1f deg");
    drawResponseCurve("boost_camera",app.boostCamera.curve);
    ImGui::EndDisabled();
    save(gameplay::view::MotionSection::Camera);ImGui::TreePop();}
    if(ImGui::TreeNode("Directional movement tilt")){
    auto& tilt=app.directionalTilt;
    ImGui::Checkbox("Enable directional tilt",&tilt.enabled);
    ImGui::BeginDisabled(!tilt.enabled);
    ImGui::Checkbox("Tilt camera",&tilt.camera);ImGui::SameLine();ImGui::Checkbox("Tilt viewmodel",&tilt.viewmodel);
    ImGui::SliderFloat("Tilt intensity",&tilt.intensity,0,3,"%.2fx");
    ImGui::SliderFloat("Strafe roll",&tilt.rollDegrees,-15,15,"%.1f deg");
    ImGui::SliderFloat("Forward pitch",&tilt.pitchDegrees,-15,15,"%.1f deg");
    ImGui::SliderFloat("Full tilt speed",&tilt.speed,20,1000,"%.0f cm/s");
    ImGui::SliderFloat("Tilt attack",&tilt.attack,.01f,1,"%.2f sec");
    ImGui::SliderFloat("Tilt recovery",&tilt.release,.01f,2,"%.2f sec");
    ImGui::SliderFloat("Tilt while ADS",&tilt.adsScale,0,1,"%.2fx");
    ImGui::EndDisabled();
    uiHelp("Presentation only. Does not change aim input, collision or movement physics.");
    save(gameplay::view::MotionSection::Tilt);ImGui::TreePop();}
    if(ImGui::TreeNode("Sniper zoom response")){
    uiHelp("Zoom curve time is independent of the weapon animation. Zero clip length uses weapon ADS timing. Interruptions blend from the current zoom.");
    ImGui::SliderFloat("Zoom intensity",&app.cameraControls.zoomIntensity,0,2,"%.2fx");
    uiHelp("0 = no zoom, 1 = normal weapon zoom. Saved with either zoom section.");
    const auto durationControl=[&](float& duration,float weaponDuration,const char* label){bool match=duration<=0;if(ImGui::Checkbox(label,&match))duration=match?0.f:std::max(.001f,weaponDuration);ImGui::BeginDisabled(match);float shown=match?weaponDuration:duration;if(ImGui::SliderFloat("Clip length",&shown,.001f,5,"%.3f sec")&&!match)duration=shown;ImGui::EndDisabled();};
    if(ImGui::TreeNode("Zoom in")){durationControl(app.cameraControls.zoomInDuration,app.weaponTiming.adsIn,"Match ADS-up length");drawResponseCurve("Zoom in",app.sniperZoomIn);ImGui::SliderFloat("Smoothness",&app.sniperZoomIn.smoothness,0,1);save(gameplay::view::MotionSection::ZoomIn);ImGui::TreePop();}
    if(ImGui::TreeNode("Zoom out")){durationControl(app.cameraControls.zoomOutDuration,app.weaponTiming.adsOut,"Match ADS-down length");drawResponseCurve("Zoom out",app.sniperZoomOut);ImGui::SliderFloat("Smoothness",&app.sniperZoomOut.smoothness,0,1);save(gameplay::view::MotionSection::ZoomOut);ImGui::TreePop();}
    for(auto* curve:{&app.sniperZoomIn,&app.sniperZoomOut}){curve->points[0].y=0;curve->points[curve->count-1].y=1;}
    ImGui::TreePop();}
}
