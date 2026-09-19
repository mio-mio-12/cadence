void drawDepthOfFieldControls(render::DepthOfFieldSettings& s){
    if(!ImGui::CollapsingHeader("Depth of field - bokeh"))return;
    ImGui::Checkbox("Enable depth of field",&s.enabled);ImGui::Checkbox("Blur first-person weapon/hands",&s.affectViewmodel);ImGui::Checkbox("Near/far mask preview",&s.preview);
    ImGui::DragFloat("Focus distance (cm)",&s.focusDistance,5,1,100000,"%.1f");ImGui::DragFloat("In-focus range (cm)",&s.focusRange,5,0,100000,"%.1f");
    ImGui::DragFloat("Near transition (cm)",&s.nearTransition,5,1,100000,"%.1f");ImGui::DragFloat("Far transition (cm)",&s.farTransition,10,1,100000,"%.1f");
    ImGui::SliderFloat("Near blur radius (px)",&s.nearRadius,0,40,"%.1f");ImGui::SliderFloat("Far blur radius (px)",&s.farRadius,0,40,"%.1f");
    ImGui::SliderFloat("Blur gamma",&s.gamma,.25f,32,"%.2f",ImGuiSliderFlags_Logarithmic);if(ImGui::IsItemHovered())ImGui::SetTooltip("Ctrl+click to enter higher values. No upper clamp; extreme gamma may crush or overflow highlights.");ImGui::SliderFloat("Bokeh amount",&s.bokehAmount,0,4,"%.2f");ImGui::SliderFloat("Bokeh highlight threshold",&s.bokehThreshold,0,4,"%.2f");ImGui::SliderFloat("Bokeh hollowness",&s.hollowness,0,.95f,"%.2f");
    ImGui::SliderFloat("Bokeh anamorphic ratio",&s.anamorphic,.25f,4,"%.2f");ImGui::SliderFloat("Bokeh rotation",&s.rotationDegrees,-180,180,"%.1f deg");
    ImGui::SliderInt("Aperture blades (0 = circle)",&s.blades,0,10);ImGui::SliderInt("DOF samples",&s.samples,8,96);
    int quality=s.downsample==1?0:s.downsample==4?2:1;const char* options[]={"Full resolution - highest cost","Half resolution","Quarter resolution - faster"};if(ImGui::Combo("DOF resolution",&quality,options,3))s.downsample=quality==0?1:quality==1?2:4;
    if(ImGui::Button("Reset DOF settings"))s={};uiHelp("Red = near blur, blue = far blur in preview. Near and far radius 0 disable each independently. Selectable-resolution, fixed-sample gather; no temporal noise. Large hollow bokeh may need more samples. This is not a reproduction of BO2's native shader.");s.sanitize();
}
