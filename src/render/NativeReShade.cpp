#include "render/NativeReShade.h"
#include "render/GlApi.h"
#include <fstream>
#include <algorithm>
#include <cmath>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include "reshade_api.hpp"
#endif
namespace render::reshade_native {
struct Runtime::Impl {
    std::string error,preset;std::filesystem::path root;int width{},height{};
#ifdef _WIN32
    using Create=bool(*)(reshade::api::device_api,void*,void*,void*,const char*,reshade::api::effect_runtime**);
    using Destroy=void(*)(reshade::api::effect_runtime*);using Present=void(*)(reshade::api::effect_runtime*);
    HMODULE library{};Create create{};Destroy destroy{};Present present{};reshade::api::effect_runtime* runtime{};
    HWND window{};Microsoft::WRL::ComPtr<ID3D11Device> device;Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;Microsoft::WRL::ComPtr<IDXGISwapChain> swapchain;Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;
    HANDLE interop{},object{},depthObject{};unsigned texture{},fbo{},inputFbo{},depthTexture{},depthFbo{},depthProgram{},depthVao{};float depthRatio{};
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depth;Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depthView;
    HANDLE(WINAPI* openDevice)(void*){};BOOL(WINAPI* closeDevice)(HANDLE){};
    HANDLE(WINAPI* registerObject)(HANDLE,void*,unsigned,unsigned,unsigned){};BOOL(WINAPI* unregisterObject)(HANDLE,HANDLE){};
    BOOL(WINAPI* lockObjects)(HANDLE,int,HANDLE*){};BOOL(WINAPI* unlockObjects)(HANDLE,int,HANDLE*){};
    void release(){
        if(runtime){destroy(runtime);runtime=nullptr;}
        if(depthObject){unregisterObject(interop,depthObject);depthObject=nullptr;}
        if(object){unregisterObject(interop,object);object=nullptr;}if(interop){closeDevice(interop);interop=nullptr;}
        if(fbo)glapi::DeleteFramebuffers(1,&fbo);if(inputFbo)glapi::DeleteFramebuffers(1,&inputFbo);if(texture)glDeleteTextures(1,&texture);fbo=inputFbo=texture=0;
        if(depthTexture)glDeleteTextures(1,&depthTexture);if(depthFbo)glapi::DeleteFramebuffers(1,&depthFbo);if(depthProgram)glapi::DeleteProgram(depthProgram);if(depthVao)glapi::DeleteVertexArrays(1,&depthVao);depthTexture=depthFbo=depthProgram=depthVao=0;depthView.Reset();depth.Reset();depthRatio=0;
        target.Reset();backbuffer.Reset();swapchain.Reset();context.Reset();device.Reset();if(window)DestroyWindow(window);window=nullptr;width=height=0;
    }
    bool initialize(const std::filesystem::path& directory,int w,int h,const std::string& selected){
        error.clear();root=directory;std::error_code ec;
        const auto folder=root/"ReShade";std::filesystem::create_directories(folder/"presets",ec);std::filesystem::create_directories(root/"reshade-shaders"/"Shaders",ec);std::filesystem::create_directories(root/"reshade-shaders"/"Textures",ec);if(ec){error=ec.message();return false;}
        const auto presetPath=folder/"presets"/std::filesystem::path(selected).filename();
        if(!std::filesystem::exists(presetPath,ec)){std::ofstream file(presetPath);file<<"Techniques=\nTechniqueSorting=\n";}
        const auto config=folder/"Cadence.ini";
        {std::ofstream file(config);file<<"[GENERAL]\nEffectSearchPaths="<<(root/"reshade-shaders"/"Shaders"/"**").string()<<"\nTextureSearchPaths="<<(root/"reshade-shaders"/"Textures"/"**").string()<<"\nPresetPath="<<presetPath.string()<<"\nSkipLoadingDisabledEffects=0\nPerformanceMode=0\n[INPUT]\nKeyOverlay=0,0,0,0\nKeyEffects=0,0,0,0\n[OVERLAY]\nTutorialProgress=4\n";if(!file){error="Cannot write ReShade/Cadence.ini";return false;}}
        if(!library){
            const auto path=folder/"ReShade64.dll";if(!std::filesystem::is_regular_file(path,ec)){error="ReShade/ReShade64.dll (6.8.0 full add-on build) is missing";return false;}
            // Only explicit native runtime creation. Do not hook Cadence's GL
            // swap chain, panels, capture path, or other applications.
            struct Environment {std::wstring name,previous;bool existed{};Environment(const wchar_t* n,const wchar_t* v):name(n){const auto size=GetEnvironmentVariableW(n,nullptr,0);existed=size>0;if(size){previous.resize(size);GetEnvironmentVariableW(n,previous.data(),size);previous.resize(size-1);}SetEnvironmentVariableW(n,v);}~Environment(){SetEnvironmentVariableW(name.c_str(),existed?previous.c_str():nullptr);}};
            Environment hooks(L"RESHADE_DISABLE_GRAPHICS_HOOK",L"1"),check(L"RESHADE_DISABLE_LOADING_CHECK",L"1"),base(L"RESHADE_BASE_PATH_OVERRIDE",folder.c_str());
            library=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if(!library){error="Cannot load native ReShade runtime (Windows "+std::to_string(GetLastError())+")";return false;}
            const auto version=reinterpret_cast<const char* const*>(GetProcAddress(library,"ReShadeVersion"));
            if(!version||!*version||std::string(*version)!="6.8.0"){error="Native integration requires ReShade 6.8.0 API";return false;}
            create=reinterpret_cast<Create>(GetProcAddress(library,"ReShadeCreateEffectRuntime"));destroy=reinterpret_cast<Destroy>(GetProcAddress(library,"ReShadeDestroyEffectRuntime"));present=reinterpret_cast<Present>(GetProcAddress(library,"ReShadeUpdateAndPresentEffectRuntime"));
        }
        if(!create||!destroy||!present){error="ReShade runtime has no native API (full add-on build required)";return false;}
        openDevice=reinterpret_cast<decltype(openDevice)>(glfwGetProcAddress("wglDXOpenDeviceNV"));closeDevice=reinterpret_cast<decltype(closeDevice)>(glfwGetProcAddress("wglDXCloseDeviceNV"));registerObject=reinterpret_cast<decltype(registerObject)>(glfwGetProcAddress("wglDXRegisterObjectNV"));unregisterObject=reinterpret_cast<decltype(unregisterObject)>(glfwGetProcAddress("wglDXUnregisterObjectNV"));lockObjects=reinterpret_cast<decltype(lockObjects)>(glfwGetProcAddress("wglDXLockObjectsNV"));unlockObjects=reinterpret_cast<decltype(unlockObjects)>(glfwGetProcAddress("wglDXUnlockObjectsNV"));
        if(!openDevice||!closeDevice||!registerObject||!unregisterObject||!lockObjects||!unlockObjects){error="GPU driver lacks OpenGL/D3D11 GPU sharing (WGL_NV_DX_interop2)";return false;}
        window=CreateWindowExW(0,L"STATIC",L"Cadence ReShade",WS_POPUP,0,0,w,h,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        if(!window){error="Cannot create native ReShade target";return false;}
        DXGI_SWAP_CHAIN_DESC desc{};desc.BufferDesc.Width=w;desc.BufferDesc.Height=h;desc.BufferDesc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT|DXGI_USAGE_SHADER_INPUT;desc.BufferCount=1;desc.OutputWindow=window;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
        const auto result=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&desc,&swapchain,&device,nullptr,&context);
        if(FAILED(result)){error="Cannot initialize ReShade D3D11 target";return false;}
        swapchain->GetBuffer(0,IID_PPV_ARGS(&backbuffer));interop=openDevice(device.Get());if(!interop||!backbuffer){error="Cannot open GPU texture sharing for ReShade";return false;}
        if(FAILED(device->CreateRenderTargetView(backbuffer.Get(),nullptr,&target))){error="Cannot create ReShade render target";return false;}
        glGenTextures(1,&texture);object=registerObject(interop,backbuffer.Get(),texture,GL_TEXTURE_2D,1);
        if(!object){error="Cannot register ReShade GPU texture";return false;}glapi::GenFramebuffers(1,&fbo);glapi::GenFramebuffers(1,&inputFbo);
        D3D11_TEXTURE2D_DESC depthDesc{};depthDesc.Width=w;depthDesc.Height=h;depthDesc.MipLevels=depthDesc.ArraySize=1;depthDesc.Format=DXGI_FORMAT_R32_FLOAT;depthDesc.SampleDesc.Count=1;depthDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
        if(FAILED(device->CreateTexture2D(&depthDesc,nullptr,&depth))||FAILED(device->CreateShaderResourceView(depth.Get(),nullptr,&depthView))){error="Cannot allocate ReShade depth target";return false;}
        glGenTextures(1,&depthTexture);depthObject=registerObject(interop,depth.Get(),depthTexture,GL_TEXTURE_2D,1);if(!depthObject){error="Cannot share ReShade depth target";return false;}glapi::GenFramebuffers(1,&depthFbo);glapi::GenVertexArrays(1,&depthVao);
        const char* vs="#version 330 core\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0,1);}";
        const char* ps="#version 330 core\nuniform sampler2D uDepth;uniform vec2 uPlanes;uniform bool uForeground,uSeparate;out float value;void main(){float d=texelFetch(uDepth,ivec2(gl_FragCoord.x,textureSize(uDepth,0).y-1-int(gl_FragCoord.y)),0).r;if(uForeground&&d<.0400001){d=clamp(d/.04,0.,1.);float n=uSeparate?.1:uPlanes.x,f=uSeparate?2000.:uPlanes.y;float z=2.*n*f/max(.0000001,f+n-(d*2.-1.)*(f-n));d=uPlanes.y/(uPlanes.y-uPlanes.x)*(1.-uPlanes.x/max(z,uPlanes.x));}value=clamp(d,0.,1.);}";
        const auto compile=[&](unsigned type,const char* source){auto shader=glapi::CreateShader(type);glapi::ShaderSource(shader,1,&source,nullptr);glapi::CompileShader(shader);GLint status{};glapi::GetShaderiv(shader,glapi::CompileStatus,&status);if(!status){glapi::DeleteShader(shader);return 0u;}return shader;};
        const auto vertex=compile(glapi::VertexShader,vs),fragment=compile(glapi::FragmentShader,ps);if(!vertex||!fragment){error="Cannot compile ReShade depth bridge";if(vertex)glapi::DeleteShader(vertex);if(fragment)glapi::DeleteShader(fragment);return false;}
        depthProgram=glapi::CreateProgram();glapi::AttachShader(depthProgram,vertex);glapi::AttachShader(depthProgram,fragment);glapi::LinkProgram(depthProgram);glapi::DeleteShader(vertex);glapi::DeleteShader(fragment);GLint linked{};glapi::GetProgramiv(depthProgram,glapi::LinkStatus,&linked);if(!linked){error="Cannot link ReShade depth bridge";return false;}
        if(!create(reshade::api::device_api::d3d11,device.Get(),context.Get(),swapchain.Get(),config.string().c_str(),&runtime)){error="ReShade native runtime initialization failed";return false;}
        uint32_t actualWidth{},actualHeight{};runtime->get_screenshot_width_and_height(&actualWidth,&actualHeight);if(actualWidth!=uint32_t(w)||actualHeight!=uint32_t(h)){error="ReShade target size mismatch";return false;}
        runtime->set_effects_state(true);width=w;height=h;preset=selected;return true;
    }
#endif
};
Runtime::Runtime():impl_(std::make_unique<Impl>()){}Runtime::~Runtime(){shutdown();}
void Runtime::shutdown(){
#ifdef _WIN32
    impl_->release(); // Keep the DLL loaded until process exit; avoid unloading
    // runtime worker TLS/driver function pointers while other renderers exist.
#endif
    impl_->error.clear();
}
bool Runtime::ready()const{
#ifdef _WIN32
    return impl_->runtime!=nullptr;
#else
    return false;
#endif
}
const std::string& Runtime::error()const{return impl_->error;}
bool Runtime::process(const std::filesystem::path& root,const std::string& preset,unsigned framebuffer,int width,int height,unsigned sceneDepth,float nearPlane,float farPlane,bool foreground,bool separate){
#ifdef _WIN32
    auto& s=*impl_;if(!s.runtime&&!s.error.empty())return false;if(width<1||height<1||!framebuffer)return false;GLint read{},draw{},viewport[4];GLfloat clearColor[4];glGetFloatv(GL_COLOR_CLEAR_VALUE,clearColor);glGetIntegerv(0x8CAA,&read);glGetIntegerv(0x8CA6,&draw);glGetIntegerv(GL_VIEWPORT,viewport);const bool scissor=glIsEnabled(GL_SCISSOR_TEST),srgb=glIsEnabled(0x8DB9);glDisable(GL_SCISSOR_TEST);glDisable(0x8DB9);
    GLint program{},vao{},active{},texture0{};glGetIntegerv(0x8B8D,&program);glGetIntegerv(0x85B5,&vao);glGetIntegerv(0x84E0,&active);glapi::ActiveTexture(glapi::Texture0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture0);
    const bool depthTest=glIsEnabled(GL_DEPTH_TEST),blend=glIsEnabled(GL_BLEND),cull=glIsEnabled(GL_CULL_FACE);GLboolean colorMask[4];glGetBooleanv(GL_COLOR_WRITEMASK,colorMask);
    bool ok=true;
    if(!s.runtime||s.width!=width||s.height!=height||s.root!=root){s.release();ok=s.initialize(root,width,height,preset);if(!ok)s.release();}
    if(ok){
        if(preset!=s.preset){s.runtime->set_current_preset_path((root/"ReShade"/"presets"/std::filesystem::path(preset).filename()).string().c_str());s.preset=preset;s.depthRatio=0;}
        if(!s.lockObjects(s.interop,1,&s.object)){s.error="ReShade GPU sharing lock failed";ok=false;}
        else{
            glapi::BindFramebuffer(glapi::DrawFramebuffer,s.fbo);glapi::FramebufferTexture2D(glapi::DrawFramebuffer,glapi::ColorAttachment0,GL_TEXTURE_2D,s.texture,0);
            if(glapi::CheckFramebufferStatus(glapi::DrawFramebuffer)!=glapi::FramebufferComplete){s.error="ReShade GPU framebuffer is incomplete";ok=false;}
            glapi::BindFramebuffer(glapi::ReadFramebuffer,s.inputFbo);glapi::FramebufferTexture2D(glapi::ReadFramebuffer,glapi::ColorAttachment0,GL_TEXTURE_2D,framebuffer,0);
            if(glapi::CheckFramebufferStatus(glapi::ReadFramebuffer)!=glapi::FramebufferComplete){s.error="Cadence source framebuffer is incomplete";ok=false;}
            if(ok){glapi::BlitFramebuffer(0,0,width,height,0,height,width,0,GL_COLOR_BUFFER_BIT,GL_NEAREST);}
            glapi::BindFramebuffer(glapi::Framebuffer,0);
            s.unlockObjects(s.interop,1,&s.object);
            if(ok&&s.lockObjects(s.interop,1,&s.depthObject)){
                glapi::BindFramebuffer(glapi::Framebuffer,s.depthFbo);glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::ColorAttachment0,GL_TEXTURE_2D,s.depthTexture,0);glViewport(0,0,width,height);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
                if(sceneDepth){glapi::UseProgram(s.depthProgram);glapi::BindVertexArray(s.depthVao);glapi::ActiveTexture(glapi::Texture0);glBindTexture(GL_TEXTURE_2D,sceneDepth);glapi::Uniform1i(glapi::GetUniformLocation(s.depthProgram,"uDepth"),0);glapi::Uniform2f(glapi::GetUniformLocation(s.depthProgram,"uPlanes"),nearPlane,farPlane);glapi::Uniform1i(glapi::GetUniformLocation(s.depthProgram,"uForeground"),foreground);glapi::Uniform1i(glapi::GetUniformLocation(s.depthProgram,"uSeparate"),separate);glapi::DrawArrays(GL_TRIANGLES,0,3);}else{glClearColor(1,1,1,1);glClear(GL_COLOR_BUFFER_BIT);}
                glapi::BindFramebuffer(glapi::Framebuffer,0);s.unlockObjects(s.interop,1,&s.depthObject);
                s.runtime->update_texture_bindings("DEPTH",{reinterpret_cast<std::uintptr_t>(s.depthView.Get())},{reinterpret_cast<std::uintptr_t>(s.depthView.Get())});
                const float ratio=farPlane/std::max(.000001f,nearPlane);if(std::abs(s.depthRatio-ratio)>.01f){s.depthRatio=ratio;s.runtime->set_preprocessor_definition("RESHADE_DEPTH_LINEARIZATION_FAR_PLANE",std::to_string(ratio).c_str());for(const char* name:{"RESHADE_DEPTH_INPUT_IS_REVERSED","RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN","RESHADE_DEPTH_INPUT_IS_LOGARITHMIC","RESHADE_DEPTH_INPUT_IS_MIRRORED"})s.runtime->set_preprocessor_definition(name,"0");}
            }
            else if(ok){s.error="ReShade GPU depth lock failed";ok=false;}
            if(ok){
                // Copy only effects, before the native runtime's splash/overlay.
                // render_effects also suppresses its normal second effects pass.
                auto* queue=s.runtime->get_command_queue();const reshade::api::resource_view target{reinterpret_cast<std::uintptr_t>(s.target.Get())};s.runtime->render_effects(queue->get_immediate_command_list(),target,target);queue->flush_immediate_command_list();
                if(s.lockObjects(s.interop,1,&s.object)){glapi::BindFramebuffer(glapi::ReadFramebuffer,s.fbo);glapi::BindFramebuffer(glapi::DrawFramebuffer,s.inputFbo);glapi::BlitFramebuffer(0,height,width,0,0,0,width,height,GL_COLOR_BUFFER_BIT,GL_NEAREST);glapi::BindFramebuffer(glapi::Framebuffer,0);s.unlockObjects(s.interop,1,&s.object);}else{s.error="ReShade GPU sharing return lock failed";ok=false;}
                if(ok)s.present(s.runtime);
            }
        }
    }
    if(!ok)s.release(); // Keep the original scene visible and wait for an explicit retry.
    glClearColor(clearColor[0],clearColor[1],clearColor[2],clearColor[3]);glapi::UseProgram(program);glapi::BindVertexArray(vao);glapi::ActiveTexture(glapi::Texture0);glBindTexture(GL_TEXTURE_2D,texture0);glapi::ActiveTexture(active);if(depthTest)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);if(blend)glEnable(GL_BLEND);else glDisable(GL_BLEND);if(cull)glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE);glColorMask(colorMask[0],colorMask[1],colorMask[2],colorMask[3]);
    glapi::BindFramebuffer(glapi::ReadFramebuffer,read);glapi::BindFramebuffer(glapi::DrawFramebuffer,draw);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);if(scissor)glEnable(GL_SCISSOR_TEST);if(srgb)glEnable(0x8DB9);return ok;
#else
    impl_->error="Native ReShade is available on Windows only";return false;
#endif
}
void Runtime::reload(){
#ifdef _WIN32
    impl_->error.clear();
    if(ready())impl_->runtime->reload_effect_next_frame(nullptr);
#endif
}
void Runtime::savePreset(){
#ifdef _WIN32
    if(ready())impl_->runtime->save_current_preset();
#endif
}
void Runtime::exportPreset(const std::filesystem::path& path){
#ifdef _WIN32
    if(ready())impl_->runtime->export_current_preset(path.string().c_str());
#endif
}
std::vector<Technique> Runtime::techniques()const{
    std::vector<Technique> result;
#ifdef _WIN32
    if(ready())impl_->runtime->enumerate_techniques(nullptr,[&](auto* runtime,auto handle){char name[256]{},effect[512]{};runtime->get_technique_name(handle,name);runtime->get_technique_effect_name(handle,effect);result.push_back({handle.handle,name,effect,runtime->get_technique_state(handle)});});
#endif
    return result;
}
std::vector<Uniform> Runtime::uniforms(const std::string& effect)const{
    std::vector<Uniform> result;
#ifdef _WIN32
    if(ready())impl_->runtime->enumerate_uniform_variables(effect.c_str(),[&](auto* runtime,auto handle){
        char source[64]{};if(runtime->get_annotation_string_from_uniform_variable(handle,"source",source)&&source[0])return;
        reshade::api::format type;uint32_t rows{},columns{},arrays{};runtime->get_uniform_variable_type(handle,&type,&rows,&columns,&arrays);if(rows*columns>4||arrays>1)return;
        Uniform u;u.handle=handle.handle;u.count=int(rows*columns);char name[256]{},label[256]{},ui[64]{};runtime->get_uniform_variable_name(handle,name);runtime->get_annotation_string_from_uniform_variable(handle,"ui_label",label);runtime->get_annotation_string_from_uniform_variable(handle,"ui_type",ui);u.name=name;u.label=label[0]?label:name;u.effect=effect;u.ui=ui;
        u.ranged=runtime->get_annotation_float_from_uniform_variable(handle,"ui_min",&u.minimum,1)&&runtime->get_annotation_float_from_uniform_variable(handle,"ui_max",&u.maximum,1);
        if(type==reshade::api::format::r32_typeless){bool v[4]{};runtime->get_uniform_value_bool(handle,v,u.count);for(int i=0;i<u.count;++i)u.value[i]=v[i]?1.f:0.f;u.type=3;}
        else if(type==reshade::api::format::r32_float||type==reshade::api::format::r16_float){runtime->get_uniform_value_float(handle,u.value.data(),u.count);u.type=0;}
        else if(type==reshade::api::format::r32_sint||type==reshade::api::format::r16_sint){int32_t v[4]{};runtime->get_uniform_value_int(handle,v,u.count);for(int i=0;i<u.count;++i)u.value[i]=float(v[i]);u.type=1;}
        else if(type==reshade::api::format::r32_uint||type==reshade::api::format::r16_uint){uint32_t v[4]{};runtime->get_uniform_value_uint(handle,v,u.count);for(int i=0;i<u.count;++i)u.value[i]=float(v[i]);u.type=2;}
        else return;result.push_back(std::move(u));
    });
#endif
    return result;
}
void Runtime::enableTechnique(std::uint64_t handle,bool enabled){
#ifdef _WIN32
    if(ready())impl_->runtime->set_technique_state({handle},enabled);
#endif
}
void Runtime::setUniform(const Uniform& u){
#ifdef _WIN32
    if(!ready()||u.count<1||u.count>4)return;for(int i=0;i<u.count;++i)if(!std::isfinite(u.value[i]))return;
    if(u.type==3){bool v[4]{};for(int i=0;i<u.count;++i)v[i]=u.value[i]!=0;impl_->runtime->set_uniform_value_bool({u.handle},v,u.count);}else if(u.type==0)impl_->runtime->set_uniform_value_float({u.handle},u.value.data(),u.count);else if(u.type==1){int32_t v[4]{};for(int i=0;i<u.count;++i)v[i]=int32_t(std::clamp(u.value[i],-1e9f,1e9f));impl_->runtime->set_uniform_value_int({u.handle},v,u.count);}else{uint32_t v[4]{};for(int i=0;i<u.count;++i)v[i]=uint32_t(std::clamp(u.value[i],0.f,1e9f));impl_->runtime->set_uniform_value_uint({u.handle},v,u.count);}
#endif
}
}
