#pragma once
#include <GLFW/glfw3.h>
#include <array>
#include <cstdint>
namespace render {
// Delayed timer queries: never wait for an unfinished GPU result.
class GpuTimer {
    using Gen=void(APIENTRY*)(GLsizei,GLuint*);using Del=void(APIENTRY*)(GLsizei,const GLuint*);
    using Begin=void(APIENTRY*)(GLenum,GLuint);using End=void(APIENTRY*)(GLenum);
    using Get=void(APIENTRY*)(GLuint,GLenum,GLuint*);using Get64=void(APIENTRY*)(GLuint,GLenum,std::uint64_t*);
    Gen gen_{};Del del_{};Begin begin_{};End end_{};Get get_{};Get64 get64_{};
    std::array<GLuint,4> queries_{};std::array<bool,4> pending_{};bool tried_{};int active_{-1};double milliseconds_{-1};
public:
    double milliseconds()const{return milliseconds_;}
    void reset(){if(queries_[0]&&del_)del_(4,queries_.data());queries_={};pending_={};tried_=false;active_=-1;milliseconds_=-1;}
    bool begin(){
        if(active_>=0)return false;
        if(!tried_){tried_=true;gen_=reinterpret_cast<Gen>(glfwGetProcAddress("glGenQueries"));del_=reinterpret_cast<Del>(glfwGetProcAddress("glDeleteQueries"));begin_=reinterpret_cast<Begin>(glfwGetProcAddress("glBeginQuery"));end_=reinterpret_cast<End>(glfwGetProcAddress("glEndQuery"));get_=reinterpret_cast<Get>(glfwGetProcAddress("glGetQueryObjectuiv"));get64_=reinterpret_cast<Get64>(glfwGetProcAddress("glGetQueryObjectui64v"));if(gen_&&del_&&begin_&&end_&&get_&&get64_)gen_(4,queries_.data());}
        if(!queries_[0])return false;
        for(int i=0;i<4;++i)if(pending_[i]){GLuint ready{};get_(queries_[i],0x8867,&ready);if(ready){std::uint64_t ns{};get64_(queries_[i],0x8866,&ns);const double ms=ns*1e-6;milliseconds_=milliseconds_<0?ms:milliseconds_*.9+ms*.1;pending_[i]=false;}}
        for(int i=0;i<4;++i)if(!pending_[i]){active_=i;begin_(0x88BF,queries_[i]);return true;}return false;
    }
    void end(){if(active_>=0){end_(0x88BF);pending_[active_]=true;active_=-1;}}
    struct Scope {GpuTimer& timer;bool active;explicit Scope(GpuTimer& t):timer(t),active(t.begin()){}~Scope(){if(active)timer.end();}};
};
}
