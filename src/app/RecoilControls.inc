// Kept separate from action timing UI so response edits never change clip speed.
bool drawRecoilResponseControls(weapon::Stats& stats){
    bool changed=false;auto& s=stats.recoil;
    changed|=ImGui::SliderFloat("Recoil intensity",&s.intensity,0,5,"%.2fx");
    if(ImGui::TreeNode("Recoil response curve")){
        changed|=drawResponseCurve("recoil",s);
        if(ImGui::TreeNode("Numeric curve controls")){
        changed|=ImGui::SliderFloat("Response duration",&s.duration,.04f,2.f,"%.3f sec");
        changed|=ImGui::SliderFloat("Curve smoothing",&s.smoothness,0,1);
        changed|=ImGui::SliderFloat("Pitch accumulation limit",&s.maxPitch,0,35,"%.1f deg");
        changed|=ImGui::SliderFloat("Yaw accumulation limit",&s.maxYaw,0,35,"%.1f deg");
        changed|=ImGui::SliderFloat("Roll accumulation limit",&s.maxRoll,0,20,"%.1f deg");
        changed|=ImGui::DragFloatRange2("Roll per shot",&s.rollMin,&s.rollMax,.01f,-10,10);
        changed|=ImGui::SliderFloat("Authored camera motion",&s.authoredCameraScale,0,2,"%.2fx");
        float values[100];for(int i=0;i<100;++i)values[i]=s.sample(i/99.f);ImGui::PlotLines("Response",values,100,0,nullptr,-1,2,{0,100});
        for(int i=1;i<s.count-1;++i){ImGui::PushID(i);changed|=ImGui::SliderFloat("Time",&s.points[i].x,s.points[i-1].x+.001f,s.points[i+1].x-.001f,"%.3f");changed|=ImGui::SliderFloat("Amount",&s.points[i].y,-1,2,"%.3f");ImGui::SameLine();if(ImGui::SmallButton("Remove")){for(int j=i;j<s.count-1;++j)s.points[j]=s.points[j+1];--s.count;changed=true;ImGui::PopID();break;}ImGui::PopID();}
        ImGui::BeginDisabled(s.count>=16);if(ImGui::Button("Add curve point")){int gap=1;for(int i=2;i<s.count;++i)if(s.points[i].x-s.points[i-1].x>s.points[gap].x-s.points[gap-1].x)gap=i;const float t=(s.points[gap].x+s.points[gap-1].x)*.5f,value=s.sample(t);for(int i=s.count;i>gap;--i)s.points[i]=s.points[i-1];s.points[gap]={t,value};++s.count;changed=true;}ImGui::EndDisabled();ImGui::SameLine();if(ImGui::Button("Reset response")){s={};changed=true;}
        uiHelp("Time is normalized across the response. Endpoints stay at zero to avoid snaps. Negative amounts allow a small settling dip. Hip/ADS center speed scales recovery; authored animation and procedural recoil are mixed.");
        ImGui::TreePop();}
        s.sanitize();ImGui::TreePop();
    }return changed;
}
