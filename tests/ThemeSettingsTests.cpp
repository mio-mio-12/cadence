#include "app/UIAppearance.h"
#include "content/DemoSets.h"
#include <array>
#include <chrono>
#include <iostream>
#include <vector>
#include <cstdio>
struct AppState {
  int colorScheme{}, fontIndex{};
  float interfaceScale{1};
  std::array<char,64> colorSchemeName{};
  std::vector<std::pair<std::string,ImFont*>> interfaceFonts;
  std::string status;
};
const auto themeTestRoot=std::filesystem::temp_directory_path()/("cadence-theme-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
std::filesystem::path colorSchemeDirectory(){return themeTestRoot;}
#include "../src/app/ThemeSettings.inc"
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){
  ImGui::CreateContext();
  try{
    std::filesystem::create_directories(themeTestRoot);
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={800,1000};io.DeltaTime=1.f/60;
    io.Fonts->AddFontDefault();unsigned char* pixels{};int w{},h{};io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    AppState app;
    ImGui::StyleColorsDark();const auto original=ImGui::GetStyle();
    applyCadenceTheme(0);
    for(int i=0;i<ImGuiCol_COUNT;++i)check(!memcmp(&original.Colors[i],&ImGui::GetStyle().Colors[i],sizeof(ImVec4)),"default palette changed");
    for(int n=0;n<int(std::size(themes::presets));++n){
      app.colorScheme=n;applyCadenceTheme(n);
      for(const auto& c:ImGui::GetStyle().Colors)for(float v:{c.x,c.y,c.z,c.w})check(std::isfinite(v)&&v>=0&&v<=1,"invalid preset color");
      ImGui::NewFrame();ImGui::Begin("Settings");ImGui::SetNextItemOpen(true,ImGuiCond_Always);drawThemeSettings(app);ImGui::End();ImGui::Render();
      check(ImGui::GetDrawData()!=nullptr,"theme UI did not render");
    }
    app.colorScheme=4;app.interfaceScale=1.3f;ImGui::GetStyle().FrameRounding=7;applyCadenceTheme(4);
    const auto saved=ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
    const auto path=themeTestRoot/"custom.cadencetheme";
    check(saveCadenceTheme(app,path),"save failed");
    applyCadenceTheme(1);app.interfaceScale=1;ImGui::GetStyle().FrameRounding=0;
    check(loadCadenceTheme(app,path),"load failed");
    check(app.colorScheme==4&&std::abs(app.interfaceScale-1.3f)<.001f&&ImGui::GetStyle().FrameRounding==7,"theme size/rounding not restored");
    check(ImGui::GetStyle().Colors[ImGuiCol_WindowBg].x==saved.x,"theme colors not restored");
    const auto broken=themeTestRoot/"broken.cadencetheme";
    {std::ofstream out(broken);out<<"CADENCETHEME 1\n1 0 0 1\n";}
    check(!loadCadenceTheme(app,broken),"truncated legacy theme accepted");
    check(ImGui::GetStyle().Colors[ImGuiCol_WindowBg].x==saved.x,"failed load changed style");
    std::filesystem::remove_all(themeTestRoot);
    ImGui::DestroyContext();std::cout<<"Theme presets, UI frames, save/load and failed-load preservation passed\n";return 0;
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';ImGui::DestroyContext();return 1;}
}
