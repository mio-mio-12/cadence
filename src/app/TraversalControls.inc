if(ImGui::CollapsingHeader("Player traversal")){
    auto& s=app.traversalSettings;
    bool changed=ImGui::SliderFloat("Minimum grab height",&s.minimumEyePercent,0,120,"%.1f%% eye height",ImGuiSliderFlags_AlwaysClamp);
    changed|=ImGui::SliderFloat("Maximum grab height",&s.maximumEyePercent,0,150,"%.1f%% eye height",ImGuiSliderFlags_AlwaysClamp);
    changed|=ImGui::SliderFloat("Grab reach",&s.reachIw,0,60,"%.1f IW units",ImGuiSliderFlags_AlwaysClamp);
    changed|=ImGui::SliderFloat("Facing cone",&app.actorMantleFacingConeDegrees,0,55,"%.0f deg",ImGuiSliderFlags_AlwaysClamp);
    ImGui::BeginDisabled(app.movementAlgorithm==3);
    changed|=ImGui::Checkbox("Airborne edge clearance",&s.airEdgeEnabled);
    ImGui::BeginDisabled(!s.airEdgeEnabled);
    changed|=ImGui::SliderFloat("Edge clearance per jump",&s.airEdgeIw,0,8,"%.2f IW units",ImGuiSliderFlags_AlwaysClamp);
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::Checkbox("Traversal visualizer",&app.traversalVisualizer);
    if(ImGui::Button("Reset traversal ranges")){s={};app.actorMantleFacingConeDegrees=28;changed=true;}
    if(changed){s.maximumEyePercent=std::max(s.minimumEyePercent,s.maximumEyePercent);app.traversalDebug.time=-100;}
    const float eye=std::max(gameplay::iw::worldUnits(20),app.actorViewHeight);
    ImGui::Text("Grab: %.1f - %.1f IW above feet",eye*s.minimumEyePercent*.01f/gameplay::iw::worldUnits(1),eye*s.maximumEyePercent*.01f/gameplay::iw::worldUnits(1));
}
