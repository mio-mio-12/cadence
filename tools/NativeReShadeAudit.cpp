#include "render/NativeReShade.h"
#include "render/StageRenderer.h"
#include "render/GlApi.h"
#include <iostream>
#include <thread>
#include <chrono>
int main(int argc,char** argv){
    if(argc!=2||!glfwInit())return 2;glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);auto* window=glfwCreateWindow(640,360,"Native ReShade audit",nullptr,nullptr);if(!window)return 2;glfwMakeContextCurrent(window);
    if(!glapi::load()){std::cerr<<"GL API unavailable";return 2;}
    std::cout<<glGetString(GL_RENDERER)<<'\n';
    unsigned texture{},depth{},fbo{};glGenTextures(1,&texture);glGenTextures(1,&depth);glapi::GenFramebuffers(1,&fbo);
    render::reshade_native::Runtime runtime;const auto root=std::filesystem::absolute(argv[1]);
    auto phase=[&](const char* name,int width,int height,float exposure,int mode,float raw,bool foreground=false){
        glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,glapi::Rgba16f,width,height,0,GL_RGBA,GL_FLOAT,nullptr);
        std::vector<float> depths(width*height,raw);glBindTexture(GL_TEXTURE_2D,depth);glTexImage2D(GL_TEXTURE_2D,0,0x8CAC,width,height,0,GL_DEPTH_COMPONENT,GL_FLOAT,depths.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        for(int frame=0;frame<120;++frame){
            glapi::BindFramebuffer(glapi::Framebuffer,fbo);glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::ColorAttachment0,GL_TEXTURE_2D,texture,0);glViewport(0,0,width,height);glDisable(GL_SCISSOR_TEST);glClearColor(.2f,.4f,.6f,1);glClear(GL_COLOR_BUFFER_BIT);
            glEnable(GL_SCISSOR_TEST);glScissor(0,height/2,width,height/2);glClearColor(.6f,.3f,.1f,1);glClear(GL_COLOR_BUFFER_BIT);glDisable(GL_SCISSOR_TEST);
            if(frame==119){float before[4]{};glReadPixels(width/2,3*height/4,1,1,GL_RGBA,GL_FLOAT,before);std::cerr<<"Source top "<<before[0]<<','<<before[1]<<','<<before[2]<<'\n';}
            if(!runtime.process(root,"Audit.ini",texture,width,height,depth,.1f,1000,foreground,false)){std::cerr<<name<<": "<<runtime.error()<<'\n';return false;}
            GLint bound{};glGetIntegerv(0x8CA6,&bound);if(bound!=int(fbo)){std::cerr<<"Framebuffer state not restored\n";return false;}
            const auto techniques=runtime.techniques();if(!techniques.empty()){
                runtime.enableTechnique(techniques[0].handle,true);for(auto u:runtime.uniforms(techniques[0].effect)){if(u.name=="Exposure")u.value[0]=exposure;else if(u.name=="Mode")u.value[0]=float(mode);else continue;runtime.setUniform(u);}
                if(frame>8){float bottom[4]{},top[4]{};glReadPixels(width/2,height/4,1,1,GL_RGBA,GL_FLOAT,bottom);glReadPixels(width/2,3*height/4,1,1,GL_RGBA,GL_FLOAT,top);
                    const float gain=std::exp2(exposure),rawDepth=foreground?raw/.04f:raw,d=mode==2?rawDepth/(10000-rawDepth*9999)*1000:rawDepth;bool pass=true;
                    for(int i=0;i<3;++i){const float b=mode?d:float(i+1)*.2f*gain;const float t=mode?d:(i==0?.6f:i==1?.3f:.1f)*gain;pass&=std::abs(bottom[i]-b)<.01f&&std::abs(top[i]-t)<.01f;}
                    if(glGetError()!=GL_NO_ERROR)return false;if(pass){std::cout<<"PASS "<<name<<" bottom="<<bottom[0]<<','<<bottom[1]<<','<<bottom[2]<<" top="<<top[0]<<'\n';return true;}if(frame==119)std::cerr<<"Last pixel "<<bottom[0]<<','<<bottom[1]<<','<<bottom[2]<<" top "<<top[0]<<','<<top[1]<<','<<top[2]<<'\n';
                }
            }
            glfwPollEvents();std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::cerr<<"FAIL "<<name<<'\n';return false;
    };
    bool ok=phase("identity and orientation",640,360,0,0,.25f)&&phase("HDR exposure",640,360,1,0,.25f)&&phase("depth",640,360,0,1,.25f)&&phase("standard ReShade depth conventions",640,360,0,2,.25f)&&phase("foreground depth unpacking",640,360,0,1,.02f,true);
    if(ok){runtime.savePreset();runtime.reload();ok=phase("shader reload",640,360,0,0,.25f)&&phase("resize",320,180,0,0,.25f);}
    runtime.shutdown();runtime.shutdown();if(runtime.ready())ok=false;
    if(ok)ok=phase("disable and re-enable",640,360,0,0,.25f);
    if(ok){render::reshade_native::Runtime missing;for(int i=0;i<10;++i)if(missing.process(root/"missing-runtime","Missing.ini",texture,640,360)||missing.ready()||missing.error().find("missing")==std::string::npos)ok=false;std::cout<<(ok?"PASS":"FAIL")<<" missing-runtime fallback\n";}
    runtime.shutdown();glapi::DeleteFramebuffers(1,&fbo);glDeleteTextures(1,&texture);glDeleteTextures(1,&depth);glfwDestroyWindow(window);glfwTerminate();return ok?0:1;
}
