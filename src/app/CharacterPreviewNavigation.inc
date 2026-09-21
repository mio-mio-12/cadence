void drawCharacterPreviewNavigation(AppState& app,float width,float height){
 // A non-scrolling child prevents wheel forwarding, including the first frame
 // the pointer enters. Key ownership also excludes global viewport shortcuts.
 ImGui::BeginChild("character_view",{width,height},ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
 const auto origin=ImGui::GetCursorScreenPos();
 renderPlayermodelPreview(app,int(width),int(height));
 ImGui::Image(static_cast<ImTextureID>(app.playermodelPreviewRenderer.colorTexture()),{width,height},{0,1},{1,0});
 ImGui::SetCursorScreenPos(origin);
 ImGui::InvisibleButton("navigation",{width,height},ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonMiddle);
 const bool hovered=ImGui::IsItemHovered(),active=ImGui::IsItemActive();const auto& io=ImGui::GetIO();
 if(hovered||active){
  ImGui::SetKeyOwner(ImGuiKey_MouseWheelY,ImGui::GetItemID());
  const float cp=std::cos(app.playermodelPreviewPitch);
  const scene::Vec3 forward{cp*std::cos(app.playermodelPreviewYaw),cp*std::sin(app.playermodelPreviewYaw),std::sin(app.playermodelPreviewPitch)};
  const auto right=scene::normalize(scene::cross(forward,{0,0,1})),up=scene::cross(right,forward);
  const float tangent=std::tan(20.f*scene::kPi/180.f),aspect=width/height;
  const float baseDistance=app.characterPreviewRadius/std::sin(std::atan(tangent*std::min(1.f,aspect)))*1.05f;
  const float distance=baseDistance*app.characterPreviewZoom;
  const auto target=app.characterPreviewCenter+app.characterPreviewPan,eye=target-forward*distance;
  const bool begin=hovered&&(ImGui::IsMouseClicked(ImGuiMouseButton_Middle)||ImGui::IsMouseClicked(ImGuiMouseButton_Left));
  const float wheel=hovered?io.MouseWheel:0;
  if(begin||wheel!=0){
   const float x=2*(io.MousePos.x-origin.x)/width-1,y=1-2*(io.MousePos.y-origin.y)/height;
   const auto ray=scene::normalize(forward+right*(x*tangent*aspect)+up*(y*tangent));
   float nearest=std::numeric_limits<float>::max();
   // Bind-pose triangle picking is performed only on a gesture, never per frame.
   for(const auto& mesh:app.playermodelPreviewScene.meshes)for(std::size_t i=0;i+2<mesh.indices.size();i+=3){
    const auto a=scene::transformPoint(mesh.modelTransform,mesh.vertices[mesh.indices[i]].position),b=scene::transformPoint(mesh.modelTransform,mesh.vertices[mesh.indices[i+1]].position),c=scene::transformPoint(mesh.modelTransform,mesh.vertices[mesh.indices[i+2]].position);
    const auto e1=b-a,e2=c-a,p=scene::cross(ray,e2);const float determinant=scene::dot(e1,p);if(std::abs(determinant)<1e-8f)continue;
    const auto s=eye-a;const float u=scene::dot(s,p)/determinant;if(u<0||u>1)continue;const auto q=scene::cross(s,e1);const float v=scene::dot(ray,q)/determinant;if(v<0||u+v>1)continue;
    const float t=scene::dot(e2,q)/determinant;if(t>0&&t<nearest)nearest=t;
   }
   if(nearest<std::numeric_limits<float>::max()){
    const auto hit=eye+ray*nearest;
    if(begin&&!io.KeyShift&&!io.KeyCtrl){
     // Keep the view direction and framing unchanged when changing depth pivot.
     const float depth=scene::dot(hit-eye,forward);
     app.characterPreviewPan=eye+forward*depth-app.characterPreviewCenter;
     app.characterPreviewZoom=std::clamp(depth/baseDistance,.025f,8.f);
    }
    if(wheel!=0){const float factor=std::exp(-wheel*.12f);app.characterPreviewPan+=(hit-target)*(1-factor);}
   }
  }
  if(active){
   if(io.KeyShift){const float scale=2*distance*tangent/height;app.characterPreviewPan+=right*(-io.MouseDelta.x*scale)+up*(io.MouseDelta.y*scale);}
   else if(io.KeyCtrl)app.characterPreviewZoom=std::clamp(app.characterPreviewZoom*std::exp(io.MouseDelta.y*.01f),.025f,8.f);
   else{app.playermodelPreviewYaw-=io.MouseDelta.x*.01f;app.playermodelPreviewPitch=std::clamp(app.playermodelPreviewPitch+io.MouseDelta.y*.01f,-1.5f,1.5f);}
  }
  app.characterPreviewZoom=std::clamp(app.characterPreviewZoom*std::exp(-wheel*.12f),.025f,8.f);
  if(hovered&&ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)){app.characterPreviewZoom=1;app.characterPreviewPan={};app.playermodelPreviewYaw=scene::kPi;app.playermodelPreviewPitch=.12f;}
 }
 ImGui::EndChild();
}
