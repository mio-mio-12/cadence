#include "render/StageRenderer.h"
#include "render/NativeReShade.h"
#include "gameplay/ScorePreview.h"
#include "app/FontBrowser.h"
#include "app/UIAppearance.h"
#include "content/DemoSets.h"
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <cfloat>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#endif
struct AppState {
    gameplay::score::Settings scoreSettings;gameplay::score::State scoreState;gameplay::score::Preview scorePreview;
    std::array<char,128> scorePresetName{"Audit"};std::vector<std::pair<std::string,ImFont*>> interfaceFonts;
    std::string pendingScoreFont,status;render::StageRenderer renderer;ImVec2 lastViewportImageSize{960,720};int workspaceMode{4};
    cadence::fonts::Browser scoreFontBrowser;
    int colorScheme{},fontIndex{};float interfaceScale{1};std::array<char,64> colorSchemeName{};
    render::reshade_native::Settings nativeReShade;render::reshade_native::Runtime nativeReShadeRuntime;std::array<char,128> nativeReShadePresetName{"Scene"};
};
std::filesystem::path cadenceAssetsDirectory(){return std::filesystem::temp_directory_path()/"cadence_score_audit";}
std::string safePackageId(std::string name){return name;}
std::string lowerText(std::string text){std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return char(std::tolower(c));});return text;}
std::filesystem::path colorSchemeDirectory(){return cadenceAssetsDirectory()/"themes";}
std::filesystem::path programDirectory(){return std::filesystem::current_path()/"diagnostics/v493/native-scene";}
#include "app/ThemeSettings.inc"
#include "app/ScoreWorkspace.inc"
#include "app/NativeReShadeControls.inc"
int auditExportLayers(const std::filesystem::path& folder){
    if(!glfwInit())throw std::runtime_error("GLFW failed");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    auto* window=glfwCreateWindow(480,320,"Export layers audit",nullptr,nullptr);if(!window)throw std::runtime_error("Window failed");glfwMakeContextCurrent(window);
    render::StageRenderer renderer;std::string error;
    const auto quad=[](float x,float a,float b,scene::Vec4 color){
        scene::CastScene out;scene::Mesh mesh;mesh.name="export audit";
        mesh.color=color;mesh.vertices={{{x,a,-9},{-1,0,0}},{{x,b,-9},{-1,0,0}},{{x,b,9},{-1,0,0}},{{x,a,9},{-1,0,0}}};
        mesh.indices={0,1,2,0,2,3};out.meshes.push_back(std::move(mesh));out.bounds={{x,a,-9},{x,b,9},true};return out;
    };
    const auto rig=quad(40,-15,3,{.8f,.3f,.2f,1}),actor=quad(75,-3,28,{.1f,.5f,.8f,1});
    if(!renderer.initialize(error)||!renderer.loadScene(rig,error)||!renderer.loadAuxiliaryScenes(nullptr,&actor,nullptr,error))throw std::runtime_error(error);
    const auto vp=scene::perspective(60*scene::kPi/180,1.5f,.1f,1000)*scene::lookAt({},{1,0,0},{0,0,1});
    renderer.setFirstPersonProjection(true);renderer.setCameraDepthRange(.1f,1000);
    renderer.setEnvironmentCamera({1,0,0},{0,0,1},60,1.5f);
    std::filesystem::create_directories(folder);
    const std::vector<scene::Mat4> empty;
    for(bool stencil:{false,true})for(bool aa:{false,true}){
        render::ActorOverlaySettings overlays;overlays.outlines=stencil;
        renderer.setActorOverlays(overlays,0);
        renderer.setAntialiasing(aa);
        const auto draw=[&](bool show){renderer.render(rig,empty,vp,480,320,false,false,false,nullptr,nullptr,nullptr,&actor,&empty,show);};
        std::vector<std::uint8_t> normal,beauty,mask,referenceDepth,worldDepth,regularDepth;
        const auto depth=[&](auto& out){if(!renderer.readDepthRgba(out,.1f,1000,0,150,false,error))throw std::runtime_error(error);};
        renderer.setExportLayers(false,false);draw(false);depth(referenceDepth);
        draw(true);depth(regularDepth);if(!renderer.readColorRgba(normal,error))throw std::runtime_error(error);
        renderer.setExportLayers(true,true);draw(true);depth(worldDepth);
        std::vector<float> samples,linear;
        render::foreground_depth::ExportRange range{.1f,1000,0,150};
        if(!renderer.captureDepthSamples(samples,range,error)||
           !renderer.readDepthRawFloat(linear,.1f,1000,0,150,false,error))throw std::runtime_error(error);
        for(auto& sample:samples)sample=range.normalized(sample);
        if(samples!=linear)throw std::runtime_error("Worker depth conversion differs from synchronous depth");
        if(!renderer.readColorRgba(beauty,error)||!renderer.readIsolateMask(mask,error))throw std::runtime_error(error);
        if(worldDepth!=referenceDepth||regularDepth==worldDepth||normal!=beauty||glGetError()!=GL_NO_ERROR)throw std::runtime_error("Depth or beauty changed unexpectedly");
        for(int layer=0;layer<2;++layer){
            auto pixels=beauty;std::size_t visible=0;
            for(std::size_t i=0;i<pixels.size();i+=4){pixels[i+3]=mask[i+layer];if(pixels[i+3])++visible;else pixels[i]=pixels[i+1]=pixels[i+2]=0;}
            if(visible<100||visible>pixels.size()/8)throw std::runtime_error("Isolate coverage is missing or invalid");
            if(aa&&!renderer.savePixelsPng(folder/(layer?"playermodels.png":"viewmodel.png"),480,320,pixels,error))throw std::runtime_error(error);
        }
        std::cout<<"PASS beauty, both alpha masks and depth. MSAA "<<aa<<", stencil "<<stencil<<'\n';
    }
    renderer.shutdown();glfwDestroyWindow(window);glfwTerminate();return 0;
}
int main(int argc,char** argv){
    try{
        if(argc==3&&std::string_view(argv[1])=="--layers")return auditExportLayers(argv[2]);
        if(argc!=4)throw std::runtime_error("model.cast sky.iwi output_folder");
        if(!glfwInit())throw std::runtime_error("GLFW failed");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
        auto* window=glfwCreateWindow(1280,900,"Score preview audit",nullptr,nullptr);if(!window)throw std::runtime_error("Window failed");glfwMakeContextCurrent(window);glfwSwapInterval(0);
        ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui_ImplGlfw_InitForOpenGL(window,true);ImGui_ImplOpenGL3_Init("#version 330");
        {
            AppState app;app.interfaceFonts.push_back({"Default",ImGui::GetIO().Fonts->AddFontDefault()});
            for(const auto* name:{"segoeui.ttf","consola.ttf"})app.interfaceFonts.push_back({name,ImGui::GetIO().Fonts->AddFontFromFileTTF((std::string("C:/Windows/Fonts/")+name).c_str(),24)});
            app.scoreSettings.font="segoeui.ttf";app.scoreFontBrowser.pendingPreview="arial.ttf";
            std::string error;auto model=scene::buildScene(cast::Document::load(argv[1]),false);
            std::vector<scene::Mat4> pose;for(const auto& b:model.skeleton.bones)pose.push_back(b.restGlobal);
            if(!app.renderer.initialize(error)||!app.renderer.loadScene(model,error)||!app.renderer.setEnvironmentPanorama(argv[2],error))throw std::runtime_error(error);
            app.renderer.setViewmodelCapture(true,{0,0,0,0});if(app.renderer.nativePostEligible())throw std::runtime_error("Native effects must bypass the alpha export");app.renderer.setViewmodelCapture(false,{0,0,0,1});if(!app.renderer.nativePostEligible())throw std::runtime_error("Beauty effects unexpectedly bypassed");
            const auto center=model.bounds.center();const auto eye=center+scene::Vec3{model.bounds.radius()*.6f,-model.bounds.radius()*2.4f,model.bounds.radius()*.45f};
            app.renderer.setEnvironmentParameters(1,0,0);app.renderer.setCameraPosition(eye);app.renderer.setEnvironmentCamera(scene::normalize(center-eye),{0,0,1},45,960.f/720);
            app.renderer.render(model,pose,scene::perspective(45*scene::kPi/180,960.f/720,.1f,10000)*scene::lookAt(eye,center,{0,0,1}),960,720,false,false,false);
            const std::filesystem::path out=argv[3];std::filesystem::create_directories(out);
            app.scorePreview.playing=false;app.scorePreview.time=.7f;app.scoreSettings.shadow=true;app.scoreSettings.shadowOffset=5;app.scoreSettings.shadowSoftness=3;app.scoreSettings.glow=true;
            for(int frame=0;frame<11;++frame){
                if(frame==2)app.scoreFontBrowser.pendingPreview="calibri.ttf";
                if(frame==5)applyCadenceTheme(10);
                glfwPollEvents();processScoreFonts(app);ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();
                if(frame==8){ImGui::GetIO().AddMousePosEvent(80,18);ImGui::GetIO().AddMouseButtonEvent(0,true);}
                if(frame==9)ImGui::GetIO().AddMouseButtonEvent(0,false);
                if(frame==10)std::snprintf(app.scoreFontBrowser.search.data(),app.scoreFontBrowser.search.size(),"sego");ImGui::NewFrame();
                ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({1280,900});ImGui::Begin("Score modifier",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoSavedSettings);
                if(frame<2||frame==5)drawScoreWorkspace(app);
                else if(frame>=6){ImGui::PushID("Score fonts");drawScoreFontPicker(app);ImGui::PopID();ImGui::Separator();ImGui::SetNextItemOpen(true,ImGuiCond_Always);drawThemeSettings(app);}
                else{
                    app.scoreSettings.x=app.scoreSettings.y=app.scoreSettings.enterX=app.scoreSettings.enterY=0;
                    app.scoreSettings.shuffleFrames=0;app.scoreSettings.rotation=frame==4?35.f:0;
                    app.scoreSettings.pivot=frame==4?4:0;app.scoreSettings.anchor=frame==3?4:0;
                    app.scoreSettings.finalScale=3;app.scoreSettings.glow=true;
                    const auto origin=ImGui::GetCursorScreenPos();const ImVec2 size{1260,875};
                    ImGui::Image(static_cast<ImTextureID>(app.renderer.colorTexture()),size,{0,1},{1,0});
                    const auto before=ImGui::GetWindowDrawList()->VtxBuffer.Size;
                    drawScoreImage(app,origin,size,app.scorePreview.state(app.scoreSettings),.7,true);
                    if(ImGui::GetWindowDrawList()->VtxBuffer.Size-before<16)throw std::runtime_error("Score glyphs missing at anchor");
                }
                ImGui::End();ImGui::Render();
                using Bind=void(*)(unsigned,unsigned);reinterpret_cast<Bind>(glfwGetProcAddress("glBindFramebuffer"))(0x8D40,0);
                glViewport(0,0,1280,900);glClearColor(.1f,.1f,.1f,1);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());glFinish();
                if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Score UI GL error");
                std::vector<std::uint8_t> pixels(1280*900*4);glReadPixels(0,0,1280,900,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
                if(frame>=1&&!app.renderer.savePixelsPng(out/("score_"+std::to_string(frame)+".png"),1280,900,pixels,error))throw std::runtime_error(error);
            }
            if(std::filesystem::is_regular_file(programDirectory()/"ReShade/ReShade64.dll")){
                app.nativeReShade.enabled=true;app.nativeReShade.preset="Scene.ini";int effectFrames=0;
                for(int frame=0;frame<90;++frame){
                    app.renderer.render(model,pose,scene::perspective(45*scene::kPi/180,960.f/720,.1f,10000)*scene::lookAt(eye,center,{0,0,1}),960,720,false,false,false);
                    if(frame==0&&!app.renderer.saveColorPng(out/"native_before.png",error))throw std::runtime_error(error);
                    if(!app.nativeReShadeRuntime.process(programDirectory(),app.nativeReShade.preset,unsigned(app.renderer.colorTexture()),960,720,app.renderer.nativeDepthTexture(),app.renderer.nativeDepthNear(),app.renderer.nativeDepthFar()))throw std::runtime_error(app.nativeReShadeRuntime.error());
                    for(const auto& t:app.nativeReShadeRuntime.techniques()){app.nativeReShadeRuntime.enableTechnique(t.handle,true);for(auto u:app.nativeReShadeRuntime.uniforms(t.effect))if(u.name=="Exposure"){u.value[0]=.65f;app.nativeReShadeRuntime.setUniform(u);}++effectFrames;}
                    glfwPollEvents();std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                if(effectFrames<10||!app.renderer.saveColorPng(out/"native_after.png",error))throw std::runtime_error("Scene effect did not load: "+error);
                processScoreFonts(app);ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({1280,900});ImGui::Begin("Post-processing",nullptr,ImGuiWindowFlags_NoSavedSettings);for(const auto& t:app.nativeReShadeRuntime.techniques()){ImGui::PushID(t.effect.c_str());ImGui::PushID(t.name.c_str());ImGui::GetStateStorage()->SetBool(ImGui::GetID("Settings"),true);ImGui::PopID();ImGui::PopID();}ImGui::SetNextItemOpen(true,ImGuiCond_Always);drawNativeReShadeControls(app);ImGui::End();ImGui::Render();
                reinterpret_cast<void(*)(unsigned,unsigned)>(glfwGetProcAddress("glBindFramebuffer"))(0x8D40,0);glViewport(0,0,1280,900);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());glFinish();std::vector<std::uint8_t> pixels(1280*900*4);glReadPixels(0,0,1280,900,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());if(!app.renderer.savePixelsPng(out/"native_controls.png",1280,900,pixels,error))throw std::runtime_error(error);
                app.nativeReShadeRuntime.shutdown();std::cout<<"PASS real-scene native effect and production controls\n";
            }
            std::cout<<"PASS production score editor, POV overlay, anchors, font preview replacement/search, red theme, GL clean\n";
        }
        ImGui_ImplOpenGL3_Shutdown();ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();glfwDestroyWindow(window);glfwTerminate();return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
