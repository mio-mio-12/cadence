#include "render/BloomShader.h"
#include "render/GlApi.h"
#include <GLFW/glfw3.h>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
int main()try{
    if(!glfwInit())throw std::runtime_error("GLFW initialization failed");
    glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    auto* window=glfwCreateWindow(16,16,"Bloom safety",nullptr,nullptr);if(!window)throw std::runtime_error("No GL window");
    glfwMakeContextCurrent(window);if(!glapi::load())throw std::runtime_error("GL loading failed");
    const auto compile=[](unsigned type,const char* source){auto s=glapi::CreateShader(type);glapi::ShaderSource(s,1,&source,nullptr);glapi::CompileShader(s);int ok{};glapi::GetShaderiv(s,glapi::CompileStatus,&ok);if(!ok){char log[2048]{};glapi::GetShaderInfoLog(s,sizeof(log),nullptr,log);throw std::runtime_error(log);}return s;};
    auto vs=compile(glapi::VertexShader,"#version 330 core\nout vec2 vScreen;void main(){vScreen=vec2((gl_VertexID<<1)&2,gl_VertexID&2)*2.-1.;gl_Position=vec4(vScreen,0,1);}");
    auto fs=compile(glapi::FragmentShader,render::kKawaseFragment);auto program=glapi::CreateProgram();glapi::AttachShader(program,vs);glapi::AttachShader(program,fs);glapi::LinkProgram(program);
    int ok{};glapi::GetProgramiv(program,glapi::LinkStatus,&ok);if(!ok)throw std::runtime_error("Shader link failed");
    unsigned vao{},fbo{},source{},target{};glapi::GenVertexArrays(1,&vao);glapi::BindVertexArray(vao);glapi::GenFramebuffers(1,&fbo);glapi::BindFramebuffer(glapi::Framebuffer,fbo);
    glGenTextures(1,&target);glBindTexture(GL_TEXTURE_2D,target);glTexImage2D(GL_TEXTURE_2D,0,glapi::Rgba16f,1,1,0,GL_RGBA,GL_FLOAT,nullptr);glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::ColorAttachment0,GL_TEXTURE_2D,target,0);
    glGenTextures(1,&source);glBindTexture(GL_TEXTURE_2D,source);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glapi::UseProgram(program);glViewport(0,0,1,1);glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);
    const auto uniform=[&](const char* n){return glapi::GetUniformLocation(program,n);};
    glapi::Uniform1i(uniform("uSource"),0);glapi::Uniform1f(uniform("uAspect"),1);glapi::Uniform2f(uniform("uTexel"),1,1);
    const auto run=[&](std::array<float,4> input,bool threshold){glBindTexture(GL_TEXTURE_2D,source);glTexImage2D(GL_TEXTURE_2D,0,glapi::Rgba32f,1,1,0,GL_RGBA,GL_FLOAT,input.data());glapi::Uniform1i(uniform("uThreshold"),threshold);glapi::DrawArrays(GL_TRIANGLES,0,3);std::array<float,4> result{};glReadPixels(0,0,1,1,GL_RGBA,GL_FLOAT,result.data());for(float v:result)if(!std::isfinite(v))throw std::runtime_error("Nonfinite bloom output");return result;};
    auto finite=run({.25f,2,64,1},false);if(finite!=std::array<float,4>{.25f,2,64,1})throw std::runtime_error("Finite HDR changed");
    auto bad=run({std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),1},false);
    if(bad!=std::array<float,4>{0,0,0,1})throw std::runtime_error("Invalid HDR handling failed");
    glapi::Uniform1f(uniform("uThresholdValue"),.5f);glapi::Uniform1f(uniform("uThresholdSoftness"),0);
    auto edge=run({.5f,.5f,.5f,1},true);if(edge!=std::array<float,4>{.5f,.5f,.5f,1})throw std::runtime_error("Zero-softness threshold failed");
    if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("GL error");
    glfwDestroyWindow(window);glfwTerminate();std::cout<<"Bloom finite HDR, NaN/Inf and zero-softness checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
