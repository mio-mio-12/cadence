#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shlobj.h>
#include "app/CameraDataExport.h"
#include "app/BoundedPipeWriter.h"
#include "app/PortablePaths.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>

namespace cadence::capture {
inline std::filesystem::path videosFolder(){
    PWSTR raw{};std::filesystem::path path;
    if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Videos,0,nullptr,&raw))){path=raw;CoTaskMemFree(raw);}
    return path;
}
inline std::filesystem::path newRecordingPath(){
    const auto folder=videosFolder();if(folder.empty())return {};
    SYSTEMTIME t{};GetLocalTime(&t);wchar_t name[128]{};
    swprintf_s(name,L"Cadence_%04u-%02u-%02u_%02u-%02u-%02u_%03u_%lu.mp4",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,GetCurrentProcessId());
    return folder/name;
}
class LiveViewportRecorder {
    struct Packet {std::vector<std::uint8_t> rgba;CameraFrame camera;std::uint64_t frame{};};
    std::thread worker_;std::mutex mutex_;std::condition_variable wake_;std::deque<Packet> queue_;
    std::atomic<bool> running_{false},stopping_{false};std::atomic<std::uint64_t> written_{};
    std::string error_;BoundedPipeWriter input_;HANDLE process_{};std::size_t queuedBytes_{};
    int width_{},height_{},fps_{};std::uint64_t next_{};
    std::chrono::steady_clock::time_point started_;
    void fail(const std::string& s){std::lock_guard lock(mutex_);error_=s;stopping_=true;wake_.notify_one();}
    void encode(std::filesystem::path path,bool camera){
        CameraCsv csv;auto csvPath=path;csvPath.replace_extension(".camera.csv");
        if(camera&&!csv.open(csvPath))fail("Cannot create camera CSV");
        Packet last;bool haveLast=false;std::uint64_t output=0;
        const auto write=[&](const Packet& p){
            std::size_t offset=0;
            while(offset<p.rgba.size()){DWORD n{};const DWORD size=static_cast<DWORD>(std::min<std::size_t>(p.rgba.size()-offset,1024*1024));
                if(!input_.write(p.rgba.data()+offset,size,n)||n==0)return false;offset+=n;}
            if(camera&&!csv.write(output,double(output)/fps_,p.camera))return false;
            written_=++output;return true;
        };
        for(;;){Packet p;
            {std::unique_lock lock(mutex_);wake_.wait(lock,[&]{return stopping_||!queue_.empty();});
                if(queue_.empty())break;p=std::move(queue_.front());queue_.pop_front();queuedBytes_-=p.rgba.size();}
            bool ok=true;while(haveLast&&output<p.frame)if(!write(last)){ok=false;break;}
            if(ok)ok=write(p);if(!ok){
                if(input_.error()==ERROR_TIMEOUT){fail("FFmpeg stopped reading frames; recording aborted after 5 seconds");TerminateProcess(process_,1);}
                else fail("FFmpeg/camera writer failed; see recording .log");break;
            }
            last=std::move(p);haveLast=true;
        }
        csv.close();input_.close();
        // EOF finalizes MP4. A wedged encoder must not keep shutdown blocked indefinitely.
        if(WaitForSingleObject(process_,30000)==WAIT_TIMEOUT){TerminateProcess(process_,1);fail("Encoder finalization timed out");WaitForSingleObject(process_,2000);}
        DWORD code{};GetExitCodeProcess(process_,&code);if(code!=0)fail("FFmpeg failed; see recording .log");CloseHandle(process_);process_=nullptr;
        {std::lock_guard lock(mutex_);queue_.clear();queuedBytes_=0;}running_=false;
    }
public:
    ~LiveViewportRecorder(){stop();if(worker_.joinable())worker_.join();}
    bool active() const{return running_;}
    bool stopping() const{return stopping_;}
    std::uint64_t frames()const{return written_;}
    std::string error(){std::lock_guard lock(mutex_);return error_;}
    void stop(){stopping_=true;wake_.notify_one();}
    bool start(const std::filesystem::path& path,int width,int height,int fps,int crf,int gop,bool camera){
        if(active())return false;if(worker_.joinable())worker_.join();
        {std::lock_guard lock(mutex_);error_.clear();}
        if(path.empty()||std::filesystem::exists(path)||width<=0||height<=0||width>8192||height>8192||fps<1||fps>240||crf<0||crf>51||gop<1||gop>10000){fail("Invalid recording path or settings");return false;}
        const auto encoder=portable::ffmpegExecutable();if(encoder.empty()){fail("FFmpeg missing: place ffmpeg.exe in the tools folder beside Cadence");return false;}const auto* exe=encoder.c_str();
        SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE read{};
        if(!input_.open(read)){fail("Cannot create encoder pipe");return false;}
        auto logPath=path;logPath.replace_extension(".log");HANDLE log=CreateFileW(logPath.c_str(),GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(log==INVALID_HANDLE_VALUE){CloseHandle(read);input_.close();fail("Cannot create recording log");return false;}
        std::wstring command=L"\""+std::wstring(exe)+L"\" -hide_banner -loglevel warning -n -f rawvideo -pixel_format rgba -video_size "+std::to_wstring(width)+L"x"+std::to_wstring(height)+L" -framerate "+std::to_wstring(fps)+L" -i pipe:0 -vf \"vflip,pad=ceil(iw/2)*2:ceil(ih/2)*2\" -an -c:v libx264 -preset veryfast -threads 2 -crf "+std::to_wstring(crf)+L" -g "+std::to_wstring(gop)+L" -keyint_min "+std::to_wstring(gop)+L" -sc_threshold 0 -pix_fmt yuv420p -movflags +faststart \""+path.wstring()+L"\"";
        STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;si.hStdInput=read;si.hStdOutput=si.hStdError=log;PROCESS_INFORMATION pi{};
        const bool ok=CreateProcessW(exe,command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)!=0;
        CloseHandle(read);CloseHandle(log);if(!ok){input_.close();fail("Cannot start FFmpeg");return false;}CloseHandle(pi.hThread);
        process_=pi.hProcess;width_=width;height_=height;fps_=fps;next_=0;written_=0;stopping_=false;running_=true;started_=std::chrono::steady_clock::now();
        try{worker_=std::thread([this,path,camera]{encode(path,camera);});}
        catch(...){input_.close();TerminateProcess(process_,1);CloseHandle(process_);process_=nullptr;running_=false;fail("Cannot start recording worker");return false;}
        return true;
    }
    bool due()const{return active()&&!stopping_&&frameAt(std::chrono::duration<double>(std::chrono::steady_clock::now()-started_).count(),fps_)>=next_;}
    bool submit(std::vector<std::uint8_t> pixels,CameraFrame camera){
        if(!active()||stopping_)return false;
        if(camera.width!=width_||camera.height!=height_||pixels.size()!=std::size_t(width_)*height_*4){fail("Viewport resized: recording finalized; start again at the new size");return false;}
        const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-started_).count();
        const auto frame=next_==0?0:frameAt(seconds,fps_);
        if(frame>next_+std::uint64_t(fps_)*5){fail("Recording stopped after a long rendering stall");return false;}
        camera.sourceTime=seconds;
        {std::lock_guard lock(mutex_);if(queue_.size()>=4||queuedBytes_+pixels.size()>96u*1024u*1024u){error_="Encoder cannot keep up; recording finalized. Lower FPS or render %.";stopping_=true;wake_.notify_one();return false;}
            queuedBytes_+=pixels.size();queue_.push_back({std::move(pixels),camera,frame});}
        next_=frame+1;wake_.notify_one();return true;
    }
};
}
