#include "render/StageRenderer.h"
#include "render/GlApi.h"
#include "render/DepthOfFieldShader.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL "<<__LINE__<<" "<<#x<<std::endl;return 1;}}while(false)
int main(){
    CHECK(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    auto* window=glfwCreateWindow(32,32,"DOF numeric check",nullptr,nullptr);CHECK(window);glfwMakeContextCurrent(window);
    render::StageRenderer renderer;std::string error;CHECK(renderer.initialize(error));
    const char* vertex="#version 330 core\nout vec2 vScreen;void main(){vScreen=vec2((gl_VertexID<<1)&2,gl_VertexID&2)*2.-1.;gl_Position=vec4(vScreen,0,1);}";
    const auto compile=[](unsigned type,const char* source){const auto shader=glapi::CreateShader(type);glapi::ShaderSource(shader,1,&source,nullptr);glapi::CompileShader(shader);return shader;};
    const auto vs=compile(0x8B31,vertex),fs=compile(0x8B30,render::kDofFragment),program=glapi::CreateProgram();
    glapi::AttachShader(program,vs);glapi::AttachShader(program,fs);glapi::LinkProgram(program);
    int linked{};glapi::GetProgramiv(program,0x8B82,&linked);if(!linked){char log[8192]{};glapi::GetProgramInfoLog(program,8192,nullptr,log);std::cerr<<log;}CHECK(linked);
    unsigned vao{},fbo{},textures[3]{};glapi::GenVertexArrays(1,&vao);glapi::BindVertexArray(vao);glapi::GenFramebuffers(1,&fbo);glapi::BindFramebuffer(glapi::Framebuffer,fbo);glGenTextures(3,textures);
    std::vector<float> pixels(32*32*4,1);for(int n=0;n<3;++n){glapi::ActiveTexture(glapi::Texture0+n);glBindTexture(GL_TEXTURE_2D,textures[n]);glTexImage2D(GL_TEXTURE_2D,0,glapi::Rgba32f,32,32,0,GL_RGBA,GL_FLOAT,pixels.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,glapi::ClampToEdge);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,glapi::ClampToEdge);}
    glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::ColorAttachment0,GL_TEXTURE_2D,textures[2],0);CHECK(glapi::CheckFramebufferStatus(glapi::Framebuffer)==glapi::FramebufferComplete);
    glapi::UseProgram(program);glViewport(0,0,32,32);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);
    const auto f=[&](const char* name,float value){glapi::Uniform1f(glapi::GetUniformLocation(program,name),value);};
    const auto i=[&](const char* name,int value){glapi::Uniform1i(glapi::GetUniformLocation(program,name),value);};
    i("uSource",0);i("uDepth",1);i("uSamples",10);i("uBlades",6);
    glapi::Uniform2f(glapi::GetUniformLocation(program,"uTexel"),1.f/32,1.f/32);
    f("uNear",1);f("uFar",10000);f("uFocus",1);f("uRange",0);f("uFarTransition",1);f("uNearTransition",1);f("uFarRadius",4.4f);f("uNearRadius",0);f("uBokeh",.5f);f("uThreshold",.8f);f("uAnamorphic",1.51f);f("uHollow",.95f);
    int failures=0;
    for(float gamma:{1.f,2.f,4.f,32.f,128.f})for(float intensity:{0.f,.0001f,.02f,.4f,1.f,4.f,10.f,1000.f}){
        f("uGamma",gamma);std::fill(pixels.begin(),pixels.end(),intensity);
        glapi::ActiveTexture(glapi::Texture0);glBindTexture(GL_TEXTURE_2D,textures[0]);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,32,32,GL_RGBA,GL_FLOAT,pixels.data());
        glapi::DrawArrays(GL_TRIANGLES,0,3);glReadPixels(0,0,32,32,GL_RGBA,GL_FLOAT,pixels.data());CHECK(glGetError()==GL_NO_ERROR);
        float maxError=0;std::size_t invalid=0;for(std::size_t p=0;p<pixels.size();++p)if(p%4!=3){if(!std::isfinite(pixels[p]))++invalid;maxError=std::max(maxError,std::abs(pixels[p]-intensity));}
        std::cout<<"gamma="<<gamma<<" input="<<intensity<<" invalid="<<invalid<<" max error="<<maxError<<std::endl;
        if(invalid||maxError>=std::max(1e-7f,intensity*.001f))++failures;
    }
    // A bright colored highlight must not poison zero/dark sibling channels.
    for(float gamma:{4.f,32.f,128.f}){
        f("uGamma",gamma);const float rgb[]={0,4,.02f};
        for(std::size_t p=0;p<pixels.size();++p)pixels[p]=p%4==3?1:rgb[p%4];
        glapi::ActiveTexture(glapi::Texture0);glBindTexture(GL_TEXTURE_2D,textures[0]);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,32,32,GL_RGBA,GL_FLOAT,pixels.data());
        glapi::DrawArrays(GL_TRIANGLES,0,3);glReadPixels(0,0,32,32,GL_RGBA,GL_FLOAT,pixels.data());CHECK(glGetError()==GL_NO_ERROR);
        for(std::size_t p=0;p<pixels.size();++p)if(p%4!=3)CHECK(std::isfinite(pixels[p])&&std::abs(pixels[p]-rgb[p%4])<1e-4f);
    }
    glapi::DeleteProgram(program);glapi::DeleteShader(vs);glapi::DeleteShader(fs);glDeleteTextures(3,textures);glapi::DeleteFramebuffers(1,&fbo);glapi::DeleteVertexArrays(1,&vao);
    renderer.shutdown();glfwDestroyWindow(window);glfwTerminate();CHECK(failures==0);std::cout<<"PASS finite HDR/dark DOF and constant-color preservation"<<std::endl;
}
