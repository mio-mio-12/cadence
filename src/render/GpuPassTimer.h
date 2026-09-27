#pragma once
#include <GLFW/glfw3.h>
#include <array>
#include <cstdint>

namespace render {
// Opt-in diagnostics only. Timestamp queries can coexist with the whole-frame
// elapsed query. Results are read only after every timestamp is available.
class GpuPassTimer {
public:
    static constexpr int capacity=24;
    struct Sample { const char* name{}; double milliseconds{}; };
    struct Result { std::uint64_t frame{}; int count{}; std::array<Sample,capacity> samples{}; };
    void setEnabled(bool enabled){enabled_=enabled;}
    const Result& result()const{return result_;}
    void reset(){
        if(del_)for(auto& f:frames_)if(f.queries[0])del_(capacity+1,f.queries.data());
        frames_={};active_=-1;tried_=false;serial_=0;result_={};
    }
    void begin(){
        if(!enabled_||active_>=0)return;
        if(!tried_){
            tried_=true;
            gen_=reinterpret_cast<Gen>(glfwGetProcAddress("glGenQueries"));
            del_=reinterpret_cast<Del>(glfwGetProcAddress("glDeleteQueries"));
            stamp_=reinterpret_cast<Stamp>(glfwGetProcAddress("glQueryCounter"));
            get_=reinterpret_cast<Get>(glfwGetProcAddress("glGetQueryObjectuiv"));
            get64_=reinterpret_cast<Get64>(glfwGetProcAddress("glGetQueryObjectui64v"));
            if(gen_&&del_&&stamp_&&get_&&get64_)for(auto& f:frames_)gen_(capacity+1,f.queries.data());
        }
        if(!frames_[0].queries[0])return;
        for(auto& f:frames_)if(f.pending){
            bool complete=true;
            for(int i=0;i<=f.count;++i){GLuint ready{};get_(f.queries[i],0x8867,&ready);if(!ready){complete=false;break;}}
            if(!complete)continue;
            Result next;next.frame=f.frame;next.count=f.count;
            std::uint64_t previous{};get64_(f.queries[0],0x8866,&previous);
            for(int i=0;i<f.count;++i){
                std::uint64_t now{};get64_(f.queries[i+1],0x8866,&now);
                next.samples[i]={f.names[i],now>=previous?(now-previous)*1e-6:0.0};previous=now;
            }
            if(next.frame>result_.frame)result_=next;
            f.pending=false;
        }
        ++serial_;
        for(int i=0;i<int(frames_.size());++i)if(!frames_[i].pending){
            active_=i;auto& f=frames_[i];f.count=0;f.frame=serial_;stamp_(f.queries[0],0x8E28);return;
        }
        // A busy GPU drops a profiling sample instead of stalling rendering.
    }
    void mark(const char* completedStage){
        if(active_<0)return;
        auto& f=frames_[active_];if(f.count>=capacity)return;
        f.names[f.count]=completedStage;stamp_(f.queries[++f.count],0x8E28);
    }
    void end(){if(active_>=0){mark("Finish");frames_[active_].pending=true;active_=-1;}}
    struct Scope {GpuPassTimer& timer;explicit Scope(GpuPassTimer& t):timer(t){timer.begin();}~Scope(){timer.end();}};
private:
    using Gen=void(APIENTRY*)(GLsizei,GLuint*);using Del=void(APIENTRY*)(GLsizei,const GLuint*);
    using Stamp=void(APIENTRY*)(GLuint,GLenum);using Get=void(APIENTRY*)(GLuint,GLenum,GLuint*);
    using Get64=void(APIENTRY*)(GLuint,GLenum,std::uint64_t*);
    struct Frame {std::array<GLuint,capacity+1> queries{};std::array<const char*,capacity> names{};std::uint64_t frame{};int count{};bool pending{};};
    std::array<Frame,4> frames_{};Result result_{};
    Gen gen_{};Del del_{};Stamp stamp_{};Get get_{};Get64 get64_{};
    bool enabled_{},tried_{};int active_{-1};std::uint64_t serial_{};
};
}
