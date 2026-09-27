#include "render/GlApi.h"
#include "render/HbaoShader.h"
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int W=64,H=48;
void require(bool v,const char* text){if(!v)throw std::runtime_error(text);}
GLuint shader(GLenum kind,const std::string& source){
 auto s=glapi::CreateShader(kind);const char* p=source.c_str();glapi::ShaderSource(s,1,&p,nullptr);glapi::CompileShader(s);
 GLint ok{};glapi::GetShaderiv(s,glapi::CompileStatus,&ok);if(!ok){char log[8192]{};glapi::GetShaderInfoLog(s,sizeof(log),nullptr,log);throw std::runtime_error(log);}return s;
}
GLuint program(const std::string& fragment){
 auto v=shader(glapi::VertexShader,"#version 330 core\nout vec2 vScreen;void main(){vScreen=vec2((gl_VertexID<<1)&2,gl_VertexID&2)*2.0-1.0;gl_Position=vec4(vScreen,0,1);}");
 auto f=shader(glapi::FragmentShader,fragment),p=glapi::CreateProgram();glapi::AttachShader(p,v);glapi::AttachShader(p,f);glapi::LinkProgram(p);GLint ok{};glapi::GetProgramiv(p,glapi::LinkStatus,&ok);require(ok,"link");glapi::DeleteShader(v);glapi::DeleteShader(f);return p;
}
void replaceOne(std::string& s,const std::string& from,const std::string& to){auto at=s.find(from);require(at!=std::string::npos,"source anchor missing");require(s.find(from,at+from.size())==std::string::npos,"ambiguous source anchor");s.replace(at,from.size(),to);}
GLuint texture(GLint format,GLenum channels,const float* data){GLuint t{};glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);glTexImage2D(GL_TEXTURE_2D,0,format,W,H,0,channels,GL_FLOAT,data);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,glapi::ClampToEdge);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,glapi::ClampToEdge);return t;}
void equal(const std::vector<float>& a,const std::vector<float>& b){require(a.size()==b.size(),"size");for(size_t i=0;i<a.size();++i)require(a[i]==b[i]||(std::isnan(a[i])&&std::isnan(b[i])),"AO/depth mismatch");}
void benchmark(const std::array<GLuint,2>& programs){
 W=1920;H=1080;glViewport(0,0,W,H);
 const auto gen=reinterpret_cast<void(APIENTRY*)(GLsizei,GLuint*)>(glfwGetProcAddress("glGenQueries"));
 const auto begin=reinterpret_cast<void(APIENTRY*)(GLenum,GLuint)>(glfwGetProcAddress("glBeginQuery"));
 const auto end=reinterpret_cast<void(APIENTRY*)(GLenum)>(glfwGetProcAddress("glEndQuery"));
 const auto result=reinterpret_cast<void(APIENTRY*)(GLuint,GLenum,unsigned long long*)>(glfwGetProcAddress("glGetQueryObjectui64v"));
 const auto del=reinterpret_cast<void(APIENTRY*)(GLsizei,const GLuint*)>(glfwGetProcAddress("glDeleteQueries"));
 require(gen&&begin&&end&&result&&del,"timer queries");GLuint query{};gen(1,&query);
 glapi::ActiveTexture(glapi::Texture0);const std::array<GLuint,2> outputs{texture(0x8230,0x8227,nullptr),texture(0x8230,0x8227,nullptr)};
 for(int fixture=0;fixture<3;++fixture){
  const float radius=fixture==0?239.687485f:1.f;std::vector<float> depths(W*H);size_t subpixel{};
  for(int y=0;y<H;++y)for(int x=0;x<W;++x){float z=fixture==2?150.f+float(x)*.25f:500.f+float(y)*4.f+(x>W/2?700.f:0.f);float raw=(100000.f+1.f-200000.f/z)/(100000.f-1.f)*.5f+.5f;depths[y*W+x]=raw;subpixel+=radius/z*H*.5f<1.f;}
  glapi::ActiveTexture(glapi::Texture0);auto depth=texture(glapi::R32f,GL_RED,depths.data());
  for(int blur:{0,4}){std::vector<float> baseline;
   for(int iteration=0;iteration<8;++iteration){const int variant=(iteration+iteration/4)%2;auto p=programs[variant];glapi::UseProgram(p);
    auto i=[&](const char*n,int v){glapi::Uniform1i(glapi::GetUniformLocation(p,n),v);};auto f=[&](const char*n,float v){glapi::Uniform1f(glapi::GetUniformLocation(p,n),v);};auto v=[&](const char*n,float x,float y){glapi::Uniform2f(glapi::GetUniformLocation(p,n),x,y);};
    i("uDepth",0);i("uAo",1);i("uForeground",0);i("uWeaponBackgroundHalo",0);i("uDirections",16);i("uSteps",3);i("uBlurRadius",blur);f("uNear",1);f("uFar",100000);f("uRadius",radius);f("uMaxPixels",160);f("uBias",.174532925f);f("uFalloff",1);f("uSharpness",16);v("uTexel",1.f/W,1.f/H);v("uInvProjection",1,1);v("uViewmodelDepthRange",.1f,10);
    glapi::ActiveTexture(glapi::Texture0);glBindTexture(GL_TEXTURE_2D,depth);
    auto draw=[&]{for(int pass=0;pass<(blur?3:1);++pass){int target=pass==1?1:0;i("uMode",pass?1:0);v("uBlurAxis",pass==1?1.f:0.f,pass==2?1.f:0.f);glapi::ActiveTexture(glapi::Texture0+1);glBindTexture(GL_TEXTURE_2D,pass?outputs[1-target]:0);glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::ColorAttachment0,GL_TEXTURE_2D,outputs[target],0);glapi::DrawArrays(GL_TRIANGLES,0,3);}};
    for(int frame=0;frame<12;++frame)draw();glFinish();
    constexpr int frames=60;auto start=std::chrono::steady_clock::now();begin(0x88BF,query);for(int frame=0;frame<frames;++frame)draw();end(0x88BF);glFinish();auto stop=std::chrono::steady_clock::now();unsigned long long nanos{};result(query,0x8866,&nanos);
    std::vector<float> pixels(W*H*2);glReadPixels(0,0,W,H,0x8227,GL_FLOAT,pixels.data());if(iteration==0)baseline=pixels;else equal(baseline,pixels);require(glGetError()==GL_NO_ERROR,"benchmark GL error");
    std::cout<<"TIMING fixture="<<fixture<<" subpixel="<<subpixel<<" blur="<<blur<<" iteration="<<iteration<<" candidate="<<variant<<" gpu_ao_ms="<<double(nanos)/1e6/frames<<" completed_ao_ms="<<std::chrono::duration<double,std::milli>(stop-start).count()/frames<<'\n';
   }
  }glDeleteTextures(1,&depth);
 }glDeleteTextures(2,outputs.data());del(1,&query);
}
}
int main(int argc,char** argv){try{
 require(glfwInit(),"GLFW");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
 auto* window=glfwCreateWindow(W,H,"HBAO radius diagnostic",nullptr,nullptr);require(window,"window");glfwMakeContextCurrent(window);require(glapi::load(),"GL API");
 std::string candidate=render::kHbaoFragment;
 const std::string gate=" float radiusPixels=min(uMaxPixels,uRadius/(max(.001,-p.z)*uInvProjection.y)*.5/uTexel.y);\n if(radiusPixels<1){color=vec4(1,fg?p.z:-p.z,0,1);return;}\n";
 replaceOne(candidate,gate,"");
 replaceOne(candidate," vec2 lUv=", " vec3 p=positionFromRaw(uv,raw);\n"+gate+" vec2 lUv=");
 replaceOne(candidate," vec3 p=positionFromRaw(uv,raw),l=", " vec3 l=");
 const std::array<GLuint,2> programs{program(render::kHbaoFragment),program(candidate)};
 GLuint vao{},fbo{};glapi::GenVertexArrays(1,&vao);glapi::BindVertexArray(vao);glapi::GenFramebuffers(1,&fbo);glapi::BindFramebuffer(glapi::Framebuffer,fbo);glViewport(0,0,W,H);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);
 const std::array<GLuint,2> outputs{texture(0x8230,0x8227,nullptr),texture(0x8230,0x8227,nullptr)};
 size_t cases{},affected{};
 for(int fixture=0;fixture<5;++fixture){
  std::vector<float> input(W*H,.9f);
  for(int y=0;y<H;++y)for(int x=0;x<W;++x){auto& z=input[y*W+x];if(fixture==1)z=x<W/2?.9f:.97f;if(fixture==2)z=x<W/2?.025f:.92f;if(fixture==3)z=(x%7==0)?1.f:.8f+float(y)/H*.18f;if(fixture==4)z=(x==W/2)?std::numeric_limits<float>::quiet_NaN():.9f;}
  glapi::ActiveTexture(glapi::Texture0);auto depth=texture(glapi::R32f,GL_RED,input.data());
  for(float cap:{.999999f,1.f,1.000001f,16.f})for(int blur:{0,1,8})for(int degenerate:{0,1}){
   std::array<std::vector<float>,2> result;
   for(int variant=0;variant<2;++variant){
    auto p=programs[variant];glapi::UseProgram(p);auto i=[&](const char*n,int v){glapi::Uniform1i(glapi::GetUniformLocation(p,n),v);};auto f=[&](const char*n,float v){glapi::Uniform1f(glapi::GetUniformLocation(p,n),v);};auto v=[&](const char*n,float x,float y){glapi::Uniform2f(glapi::GetUniformLocation(p,n),x,y);};
    i("uDepth",0);i("uAo",1);i("uForeground",fixture==2);i("uWeaponBackgroundHalo",1);i("uDirections",8);i("uSteps",6);i("uBlurRadius",blur);f("uNear",1);f("uFar",100);f("uRadius",100);f("uMaxPixels",cap);f("uBias",.15f);f("uFalloff",1.8f);f("uSharpness",16);v("uTexel",1.f/W,1.f/H);v("uInvProjection",degenerate?0.f:1.f,1.f);v("uViewmodelDepthRange",.1f,10.f);
    glapi::ActiveTexture(glapi::Texture0);glBindTexture(GL_TEXTURE_2D,depth);
    for(int pass=0;pass<(blur?3:1);++pass){
     const int target=pass==1?1:0;i("uMode",pass?1:0);v("uBlurAxis",pass==1?1.f:0.f,pass==2?1.f:0.f);
     glapi::ActiveTexture(glapi::Texture0+1);glBindTexture(GL_TEXTURE_2D,pass?outputs[1-target]:0);
     glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::ColorAttachment0,GL_TEXTURE_2D,outputs[target],0);require(glapi::CheckFramebufferStatus(glapi::Framebuffer)==glapi::FramebufferComplete,"FBO");glapi::DrawArrays(GL_TRIANGLES,0,3);
    }
    result[variant].resize(W*H*2);glReadPixels(0,0,W,H,0x8227,GL_FLOAT,result[variant].data());require(glGetError()==GL_NO_ERROR,"GL error");
   }
   equal(result[0],result[1]);if(fixture!=4)for(size_t x=0;x<result[0].size();x+=2){require(std::isfinite(result[0][x])&&std::isfinite(result[0][x+1]),"nonfinite finite fixture");affected+=result[0][x]<.999f;}
   // Exact raw AO/depth equality implies identical deterministic composition;
   // retain explicit RGB multiplication check with nonuniform finite colors.
   for(size_t x=0;x<result[0].size();x+=2)if(std::isfinite(result[0][x]))for(float c:{.03125f,.37f,4.f})require(c*result[0][x]==c*result[1][x],"composite color");
   ++cases;
  }
  glDeleteTextures(1,&depth);
 }
 require(affected>0,"vacuous fixture: no AO contribution");
 if(argc>1&&std::string(argv[1])=="--benchmark")benchmark(programs);
 for(auto p:programs)glapi::DeleteProgram(p);glDeleteTextures(2,outputs.data());glapi::DeleteFramebuffers(1,&fbo);glapi::DeleteVertexArrays(1,&vao);glfwDestroyWindow(window);glfwTerminate();
 std::cout<<"PASS "<<cases<<" AO radius cases; contributing pixels="<<affected<<"; exact raw visibility/depth and finite composite parity\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
