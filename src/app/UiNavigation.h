#pragma once
#include <imgui.h>
#include <algorithm>
inline int uiCategory(const char* id,const char* const* labels,int count,int initial=0){
    const auto key=ImGui::GetID(id);auto* storage=ImGui::GetStateStorage();
    int selected=std::clamp(storage->GetInt(key,initial),0,count-1);
    ImGui::PushID(id);
    // Visible, wrapping page choices replace the hidden category dropdown.
    // Detailed controls retain collapsible groups only where useful.
    for(int i=0;i<count;++i){
        const float width=ImGui::CalcTextSize(labels[i]).x+ImGui::GetStyle().FramePadding.x*2;
        if(i&&ImGui::GetItemRectMax().x+ImGui::GetStyle().ItemSpacing.x+width<=ImGui::GetCursorScreenPos().x+ImGui::GetContentRegionAvail().x)ImGui::SameLine();
        if(ImGui::Selectable(labels[i],selected==i,0,{width,ImGui::GetFrameHeight()})){selected=i;storage->SetInt(key,selected);}
    }
    ImGui::PopID();ImGui::Spacing();return selected;
}
inline void uiHelp(const char* text){
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal|ImGuiHoveredFlags_AllowWhenDisabled)){
        ImGui::BeginTooltip();ImGui::PushTextWrapPos(ImGui::GetFontSize()*28.f);
        ImGui::TextUnformatted(text);ImGui::PopTextWrapPos();ImGui::EndTooltip();
    }
}
