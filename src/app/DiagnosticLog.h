#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <memory>
#include <sstream>
#include <streambuf>
#include <string_view>
#include <thread>

namespace diagnostic {
enum class Phase { Startup, Events, Unfocused, Catalog, ClassLoad, MapLoad, ModelLoad, AnimationLoad, Ui, Bots, Scene, UiFinalize, UiDraw, Present, Shutdown };
inline const char* name(Phase p){
    constexpr const char* names[]{"startup","window events","paused/unfocused","catalog","class loading","map loading","model loading","animation loading","UI/gameplay","bot simulation","scene render","UI finalization","UI draw/driver","swap/present","shutdown"};
    return names[static_cast<int>(p)];
}
class Log {
    // Existing loading stdout/stderr is retained in both the console and file.
    class Tee final:public std::streambuf {
        std::streambuf* console_;std::ofstream& file_;std::mutex mutex_;
        std::streamsize xsputn(const char* s,std::streamsize n) override {std::lock_guard guard(mutex_);console_->sputn(s,n);file_.write(s,n);return n;}
        int overflow(int c) override {if(c!=traits_type::eof()){char ch=static_cast<char>(c);xsputn(&ch,1);}return traits_type::not_eof(c);}
        int sync() override {std::lock_guard guard(mutex_);console_->pubsync();file_.flush();return file_?0:-1;}
    public:Tee(std::streambuf* console,std::ofstream& file):console_(console),file_(file){}
    };
    std::ofstream file_;std::streambuf *oldOut_{},*oldErr_{};std::unique_ptr<Tee> tee_;
    std::chrono::steady_clock::time_point started_=std::chrono::steady_clock::now();
    std::atomic<Phase> phase_{Phase::Startup};std::atomic<double> phaseStarted_{};
    std::mutex waitMutex_;std::condition_variable wait_;bool stop_{};std::thread watcher_;
public:
    explicit Log(const std::filesystem::path& path){
        std::error_code ec;if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path(),ec);
        file_.open(path,std::ios::out|std::ios::app);if(!file_)return;
        oldOut_=std::cout.rdbuf();oldErr_=std::cerr.rdbuf();tee_=std::make_unique<Tee>(oldOut_,file_);
        std::cout.rdbuf(tee_.get());std::cerr.rdbuf(tee_.get());
event("Cadence v125 verbose recording started. Close Cadence to stop. Log: "+path.string());
        event("Timings are one-second aggregates; GPU time is delayed/non-additive. No forced GPU waits. No input or screen recording.");
        watcher_=std::thread([this]{std::unique_lock lock(waitMutex_);double lastNotice=-10;while(!wait_.wait_for(lock,std::chrono::seconds(1),[this]{return stop_;})){
            const auto p=phase_.load();const double now=seconds(),age=now-phaseStarted_.load();
            if(p!=Phase::Unfocused&&age>1&&now-lastNotice>=5){lastNotice=now;event(std::string("WATCHDOG: still in ")+name(p)+" for "+std::to_string(age)+" s");}
            std::cout.flush();
        }});
    }
    ~Log(){if(!tee_)return;{std::lock_guard lock(waitMutex_);stop_=true;}wait_.notify_all();if(watcher_.joinable())watcher_.join();event("Recording stopped; normal shutdown.");std::cout.flush();std::cerr.rdbuf(oldErr_);std::cout.rdbuf(oldOut_);}
    bool good()const{return static_cast<bool>(tee_);}
    double seconds()const{return std::chrono::duration<double>(std::chrono::steady_clock::now()-started_).count();}
    void phase(Phase p){phaseStarted_.store(seconds());phase_.store(p);}
    void event(std::string_view text){std::ostringstream line;line<<"[diag +"<<std::fixed<<std::setprecision(3)<<seconds()<<"s] "<<text<<'\n';const auto s=line.str();std::cout.write(s.data(),static_cast<std::streamsize>(s.size()));}
};
inline Log* active{}; // Set before worker tasks, cleared after they finish.
inline void phase(Phase p){if(active)active->phase(p);}
inline void event(std::string_view s){if(active)active->event(s);}
struct Scope {Phase previous{Phase::Ui};explicit Scope(Phase p){phase(p);}~Scope(){phase(previous);}};
struct Frames {
    unsigned count{},over16{},over33{},over100{};double elapsed{},maximum{},pre{},ui{},finalize{},draw{},present{},nextSample{};
    void add(double ms,double preMs,double uiMs,double finalizeMs,double drawMs,double presentMs){++count;elapsed+=ms;maximum=std::max(maximum,ms);over16+=ms>16.67;over33+=ms>33.33;over100+=ms>100;pre+=preMs;ui+=uiMs;finalize+=finalizeMs;draw+=drawMs;present+=presentMs;}
    std::string summary()const{std::ostringstream o;const double n=std::max(1u,count);o<<std::fixed<<std::setprecision(3)<<"FRAME n="<<count<<" avg_ms="<<elapsed/n<<" max_ms="<<maximum<<" over16/33/100="<<over16<<'/'<<over33<<'/'<<over100<<" pre_ui_ms="<<pre/n<<" ui_build_ms="<<ui/n<<" finalize_ms="<<finalize/n<<" ui_draw_ms="<<draw/n<<" present_ms="<<present/n;return o.str();}
};
}
