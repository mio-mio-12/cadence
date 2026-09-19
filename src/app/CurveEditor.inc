// General curve editor: endpoint time is fixed but endpoint values may be dragged.
bool drawResponseCurve(const char* id,gameplay::view::ResponseCurve& curve){
    ImGui::PushID(id);
    const ImVec2 origin=ImGui::GetCursorScreenPos(),size{std::max(160.f,ImGui::GetContentRegionAvail().x),150.f};
    ImGui::InvisibleButton("canvas",size,ImGuiButtonFlags_MouseButtonLeft);
    auto* draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin,{origin.x+size.x,origin.y+size.y},IM_COL32(20,25,32,255));
    const auto screen=[&](float x,float y){return ImVec2{origin.x+x*size.x,origin.y+(3.f-y)*size.y/4.f};};
    for(int i=0;i<=4;++i){const float x=origin.x+size.x*i/4.f;draw->AddLine({x,origin.y},{x,origin.y+size.y},IM_COL32(55,60,68,255));}
    draw->AddLine(screen(0,0),screen(1,0),IM_COL32(110,110,110,255));
    for(int i=1;i<=160;++i)draw->AddLine(screen((i-1)/160.f,curve.sample((i-1)/160.f)),screen(i/160.f,curve.sample(i/160.f)),IM_COL32(90,190,245,255),2);
    const auto mouse=ImGui::GetIO().MousePos;
    int nearest=-1;float distance=100.f;
    for(int i=0;i<curve.count;++i){const auto p=screen(curve.points[i].x,curve.points[i].y);const float d=(p.x-mouse.x)*(p.x-mouse.x)+(p.y-mouse.y)*(p.y-mouse.y);if(d<distance){distance=d;nearest=i;}draw->AddCircleFilled(p,5,IM_COL32(240,190,85,255));}
    auto* storage=ImGui::GetStateStorage();const auto key=ImGui::GetID("dragged");bool changed=false;
    if(ImGui::IsItemHovered()&&ImGui::IsMouseClicked(0)){
        if(ImGui::IsMouseDoubleClicked(0)&&nearest>0&&nearest<curve.count-1){for(int i=nearest;i<curve.count-1;++i)curve.points[i]=curve.points[i+1];--curve.count;nearest=-1;changed=true;}
        else if(nearest<0&&curve.count<16){const float x=std::clamp((mouse.x-origin.x)/size.x,.001f,.999f);int at=1;while(at<curve.count-1&&curve.points[at].x<x)++at;if(x>curve.points[at-1].x+.001f&&x<curve.points[at].x-.001f){for(int i=curve.count;i>at;--i)curve.points[i]=curve.points[i-1];curve.points[at]={x,std::clamp(3.f-(mouse.y-origin.y)*4.f/size.y,-1.f,3.f)};++curve.count;nearest=at;changed=true;}}
        storage->SetInt(key,nearest);
    }
    const int active=storage->GetInt(key,-1);
    if(ImGui::IsItemActive()&&ImGui::IsMouseDown(0)&&active>=0&&active<curve.count){curve.points[active]={std::clamp((mouse.x-origin.x)/size.x,active==0?0.f:curve.points[active-1].x+.001f,active==curve.count-1?1.f:curve.points[active+1].x-.001f),std::clamp(3.f-(mouse.y-origin.y)*4.f/size.y,-1.f,3.f)};changed=true;}
    if(!ImGui::IsMouseDown(0))storage->SetInt(key,-1);
    uiHelp("Click to add | drag to move | double-click to remove. Endpoint values are editable.");
    curve.sanitize();ImGui::PopID();return changed;
}

// Normalized response editor shared by camera recoil and procedural mantle.
bool drawResponseCurve(const char* id,gameplay::view::RecoilSettings& curve){
    ImGui::PushID(id);
    const ImVec2 origin=ImGui::GetCursorScreenPos(),size{std::max(160.f,ImGui::GetContentRegionAvail().x),150.f};
    ImGui::InvisibleButton("canvas",size,ImGuiButtonFlags_MouseButtonLeft);
    auto* draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin,{origin.x+size.x,origin.y+size.y},IM_COL32(20,25,32,255));
    const auto screen=[&](float x,float y){return ImVec2{origin.x+x*size.x,origin.y+(2.f-y)*size.y/3.f};};
    for(int i=0;i<=4;++i){const float x=origin.x+size.x*i/4.f;draw->AddLine({x,origin.y},{x,origin.y+size.y},IM_COL32(55,60,68,255));}
    draw->AddLine(screen(0,0),screen(1,0),IM_COL32(110,110,110,255));
    for(int i=1;i<=160;++i)draw->AddLine(screen((i-1)/160.f,curve.sample((i-1)/160.f)),screen(i/160.f,curve.sample(i/160.f)),IM_COL32(90,190,245,255),2);
    const auto mouse=ImGui::GetIO().MousePos;
    int nearest=-1;float distance=100.f;
    for(int i=0;i<curve.count;++i){const auto p=screen(curve.points[i].x,curve.points[i].y);const float d=(p.x-mouse.x)*(p.x-mouse.x)+(p.y-mouse.y)*(p.y-mouse.y);if(d<distance){distance=d;nearest=i;}draw->AddCircleFilled(p,5,IM_COL32(240,190,85,255));}
    auto* storage=ImGui::GetStateStorage();const auto key=ImGui::GetID("dragged");bool changed=false;
    if(ImGui::IsItemHovered()&&ImGui::IsMouseClicked(0)){
        if(ImGui::IsMouseDoubleClicked(0)&&nearest>0&&nearest<curve.count-1){for(int i=nearest;i<curve.count-1;++i)curve.points[i]=curve.points[i+1];--curve.count;nearest=-1;changed=true;}
        else if(nearest<0&&curve.count<16){const float x=std::clamp((mouse.x-origin.x)/size.x,.001f,.999f);int at=1;while(at<curve.count-1&&curve.points[at].x<x)++at;if(x>curve.points[at-1].x+.001f&&x<curve.points[at].x-.001f){for(int i=curve.count;i>at;--i)curve.points[i]=curve.points[i-1];curve.points[at]={x,std::clamp(2.f-(mouse.y-origin.y)*3.f/size.y,-1.f,2.f)};++curve.count;nearest=at;changed=true;}}
        storage->SetInt(key,nearest);
    }
    const int active=storage->GetInt(key,-1);
    if(ImGui::IsItemActive()&&ImGui::IsMouseDown(0)&&active>0&&active<curve.count-1){curve.points[active]={std::clamp((mouse.x-origin.x)/size.x,curve.points[active-1].x+.001f,curve.points[active+1].x-.001f),std::clamp(2.f-(mouse.y-origin.y)*3.f/size.y,-1.f,2.f)};changed=true;}
    if(!ImGui::IsMouseDown(0))storage->SetInt(key,-1);
    uiHelp("Click to add | drag to move | double-click to remove. Endpoints are fixed.");
    curve.sanitize();ImGui::PopID();return changed;
}
