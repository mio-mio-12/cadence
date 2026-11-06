#include "render/StageRenderer.h"
#include "render/GlApi.h"
#include "render/PreparedTexture.h"
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool ok,const std::string& message){if(!ok)throw std::runtime_error(message);}
void noError(const std::string& where){const auto e=glGetError();require(e==GL_NO_ERROR,where+" GL error "+std::to_string(e));}
struct Fixture {const char* name;unsigned fourcc,linear,color;std::array<float,4> expected;};
void writeFixture(const std::filesystem::path& path,const Fixture& f){
    const unsigned payload=f.fourcc?(f.fourcc==0x31545844?8:16):64;
    std::vector<unsigned char> bytes(128+payload);
    const auto put=[&](unsigned at,unsigned value){for(unsigned i=0;i<4;++i)bytes[at+i]=static_cast<unsigned char>(value>>(8*i));};
    put(0,0x20534444);put(4,124);put(8,f.fourcc?0x81007:0x100F);put(12,4);put(16,4);put(20,f.fourcc?payload:16);put(28,1);
    put(76,32);put(80,f.fourcc?4:0x41);put(84,f.fourcc);put(108,0x1000);
    if(f.fourcc){
        const unsigned color=128+(f.fourcc==0x35545844?8:0);
        // RGB565 endpoint 0x8410 -> (132,130,132), all indices zero.
        bytes[color]=0x10;bytes[color+1]=0x84;
        if(f.fourcc==0x35545844){bytes[128]=153;bytes[129]=0;}
    }else{
        put(88,32);put(92,0xFF);put(96,0xFF00);put(100,0xFF0000);put(104,0xFF000000);
        for(unsigned i=128;i<bytes.size();i+=4){bytes[i]=128;bytes[i+1]=64;bytes[i+2]=192;bytes[i+3]=153;}
    }
    std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());require(bool(file),"fixture write failed");
}
GLuint shader(GLenum type,const char* source){
    const auto id=glapi::CreateShader(type);glapi::ShaderSource(id,1,&source,nullptr);glapi::CompileShader(id);
    GLint ok{};glapi::GetShaderiv(id,glapi::CompileStatus,&ok);if(!ok){char log[4096]{};glapi::GetShaderInfoLog(id,sizeof(log),nullptr,log);throw std::runtime_error(log);}return id;
}
float decode(float x){return x<=.04045f?x/12.92f:std::pow((x+.055f)/1.055f,2.4f);}
struct Sampler {
    GLuint program{},vao{},fbo{},target{};
    Sampler(){
        const auto vs=shader(glapi::VertexShader,"#version 330 core\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.-1.,0,1);}");
        const auto fs=shader(glapi::FragmentShader,"#version 330 core\nuniform sampler2D tex;out vec4 color;void main(){color=texture(tex,vec2(.5));}");
        program=glapi::CreateProgram();glapi::AttachShader(program,vs);glapi::AttachShader(program,fs);glapi::LinkProgram(program);
        GLint ok{};glapi::GetProgramiv(program,glapi::LinkStatus,&ok);require(ok!=0,"sampling program link failed");glapi::DeleteShader(vs);glapi::DeleteShader(fs);
        glapi::GenVertexArrays(1,&vao);glGenTextures(1,&target);glBindTexture(GL_TEXTURE_2D,target);
        glTexImage2D(GL_TEXTURE_2D,0,glapi::Rgba32f,1,1,0,GL_RGBA,GL_FLOAT,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glapi::GenFramebuffers(1,&fbo);glapi::BindFramebuffer(glapi::Framebuffer,fbo);glapi::FramebufferTexture2D(glapi::Framebuffer,glapi::ColorAttachment0,GL_TEXTURE_2D,target,0);
        require(glapi::CheckFramebufferStatus(glapi::Framebuffer)==glapi::FramebufferComplete,"float framebuffer incomplete");noError("sampler setup");
    }
    ~Sampler(){glapi::BindFramebuffer(glapi::Framebuffer,0);glapi::DeleteFramebuffers(1,&fbo);glDeleteTextures(1,&target);glapi::DeleteVertexArrays(1,&vao);glapi::DeleteProgram(program);}
    std::array<float,4> read(GLuint texture){
        glapi::BindFramebuffer(glapi::Framebuffer,fbo);glViewport(0,0,1,1);glDisable(0x8DB9); // GL_FRAMEBUFFER_SRGB
        glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_SCISSOR_TEST);glDisable(GL_CULL_FACE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        glapi::UseProgram(program);glapi::BindVertexArray(vao);glapi::ActiveTexture(glapi::Texture0);glBindTexture(GL_TEXTURE_2D,texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glapi::Uniform1i(glapi::GetUniformLocation(program,"tex"),0);glapi::DrawArrays(GL_TRIANGLES,0,3);
        std::array<float,4> value{};glReadBuffer(glapi::ColorAttachment0);glReadPixels(0,0,1,1,GL_RGBA,GL_FLOAT,value.data());noError("sample/readback");return value;
    }
};
}
int main(int argc,char** argv){
    GLFWwindow* window=nullptr;
    try{
        require(argc==2,"usage: dds_upload_check NEW_OUTPUT_DIRECTORY");const std::filesystem::path output=argv[1];
        require(!std::filesystem::exists(output),"output directory must be new (no overwrites)");std::filesystem::create_directories(output);
        require(glfwInit()!=0,"GLFW initialization failed");glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
        window=glfwCreateWindow(32,32,"DDS sampling diagnostic",nullptr,nullptr);require(window!=nullptr,"hidden GL context unavailable");glfwMakeContextCurrent(window);
        {
            render::StageRenderer renderer;std::string error;require(renderer.initialize(error),"renderer initialization: "+error);
            Sampler sampler;std::ofstream report(output/"results.csv");report<<"fixture,path,srgb,internal,r,g,b,a,status\n";
            std::ofstream info(output/"gpu.txt");info<<"Vendor: "<<glGetString(GL_VENDOR)<<"\nRenderer: "<<glGetString(GL_RENDERER)<<"\nVersion: "<<glGetString(GL_VERSION)<<"\nPrepared payloads are manually uploaded, not StageRenderer::uploadPreparedTexture coverage.\n";
            const Fixture fixtures[]={
                {"bc1",0x31545844,0x83F1,0x8C4D,{132.f/255,130.f/255,132.f/255,1}},
                {"bc3",0x35545844,0x83F3,0x8C4F,{132.f/255,130.f/255,132.f/255,153.f/255}},
                {"rgba",0,0x8058,0x8C43,{128.f/255,64.f/255,192.f/255,153.f/255}}};
            unsigned checked=0;
            for(const auto& f:fixtures){const auto path=output/(std::string(f.name)+".dds");writeFixture(path,f);
                for(const bool prepared:{false,true})for(const bool srgb:{false,true}){
                    GLuint tex{};
                    if(prepared){
                        render::texture::Request request;request.path=path;request.srgb=srgb;const auto data=render::texture::prepare(request);require(data.ready,"prepare not ready");
                        glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
                        if(data.compressed){require(glapi::CompressedTexImage2D!=nullptr,"compressed upload unsupported");glapi::CompressedTexImage2D(GL_TEXTURE_2D,0,data.internalFormat,data.width,data.height,0,static_cast<GLsizei>(data.bytes.size()),data.bytes.data());}
                        else glTexImage2D(GL_TEXTURE_2D,0,data.internalFormat,data.width,data.height,0,data.format,data.type,data.bytes.data());
                    }else tex=renderer.loadTexture(path,false,false,1024,false,srgb,false);
                    require(tex!=0,"sync load returned zero");noError("upload");glBindTexture(GL_TEXTURE_2D,tex);GLint format{},width{},height{};
                    glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&format);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&width);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&height);
                    const auto values=sampler.read(tex);bool pass=static_cast<unsigned>(format)==(srgb?f.color:f.linear)&&width==4&&height==4;
                    for(unsigned channel=0;channel<4;++channel){const float expected=srgb&&channel<3?decode(f.expected[channel]):f.expected[channel];pass=pass&&std::isfinite(values[channel])&&std::abs(values[channel]-expected)<.006f;}
                    report<<f.name<<','<<(prepared?"prepared_payload_manual_upload":"StageRenderer_sync")<<','<<srgb<<','<<format;for(auto v:values)report<<','<<v;report<<','<<(pass?"PASS":"FAIL")<<std::endl;
                    glDeleteTextures(1,&tex);require(pass,std::string(f.name)+" upload/sampling mismatch (see results.csv)");++checked;
                }
            }
            require(bool(report),"report write failed");std::cout<<"PASS "<<checked<<" actual GPU upload/sampling cases; no visual parity claim\n";
        }
        glfwDestroyWindow(window);glfwTerminate();return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';if(window)glfwDestroyWindow(window);glfwTerminate();return 1;}
}
