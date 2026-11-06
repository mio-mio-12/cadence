#pragma once
#include "app/BoundedPipeWriter.h"
#include "app/DepthProRes.h"
#include "render/ForegroundDepth.h"
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace cadence::capture {
class ProResExportStream {
public:
    using Bytes = std::vector<std::uint8_t>;
    struct Frame {
        std::shared_ptr<const Bytes> pixels;
        std::shared_ptr<const Bytes> coverage;
        std::vector<float> depth;
        std::size_t channel{};
        std::optional<render::foreground_depth::ExportRange> depthRange;
        std::size_t bytes() const {
            return (pixels?pixels->size():0)+(coverage?coverage->size():0)+depth.size()*sizeof(float);
        }
    };
    struct DepthOptions { bool dither{}; float strength{}; bool fullRange{}; };

    ~ProResExportStream(){ finish(); }
    bool start(const std::filesystem::path& encoder,std::wstring command,std::string name,DepthOptions depth) {
        name_=std::move(name); depth_=depth;
        HANDLE reader{};
        if(!input_.open(reader))return false;
        SECURITY_ATTRIBUTES inheritable{sizeof(inheritable),nullptr,TRUE};
        const HANDLE sink=CreateFileW(L"NUL",GENERIC_WRITE,FILE_SHARE_WRITE,&inheritable,OPEN_EXISTING,0,nullptr);
        STARTUPINFOW startup{};
        startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES;
        startup.hStdInput=reader;startup.hStdOutput=startup.hStdError=sink;
        PROCESS_INFORMATION process{};
        const bool opened=sink!=INVALID_HANDLE_VALUE&&
            CreateProcessW(encoder.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process);
        CloseHandle(reader);
        if(sink!=INVALID_HANDLE_VALUE)CloseHandle(sink);
        if(!opened){input_.close();return false;}
        CloseHandle(process.hThread);process_=process.hProcess;
        try {worker_=std::thread([this]{run();});}
        catch(...){input_.close();TerminateProcess(process_,1);CloseHandle(process_);return false;}
        return true;
    }
    bool submit(Frame frame,std::string& error) {
        constexpr std::size_t queueBudget=64u*1024u*1024u;
        const auto bytes=frame.bytes();
        std::unique_lock lock(mutex_);
        ready_.wait(lock,[&]{
            return ending_||queue_.empty()||(queue_.size()<2&&queuedBytes_<=queueBudget&&bytes<=queueBudget-queuedBytes_);
        });
        if(ending_){error=error_.empty()?name_+" stream is closed":error_;return false;}
        queue_.push_back(std::move(frame));
        queuedBytes_+=bytes;
        ready_.notify_all();
        return true;
    }
    void endInput() {
        std::lock_guard lock(mutex_);
        ending_=true;
        ready_.notify_all();
    }
    std::string finish() {
        endInput();
        if(worker_.joinable())worker_.join();
        return error_;
    }
private:
    void fail(std::string error) {
        std::lock_guard lock(mutex_);
        if(error_.empty())error_=name_+" "+std::move(error);
        ending_=true;
        queue_.clear();queuedBytes_=0;
        ready_.notify_all();
    }
    bool write(const void* data,std::size_t bytes) {
        const auto* ptr=static_cast<const std::uint8_t*>(data);
        while(bytes){
            DWORD written{};
            if(!input_.write(ptr,static_cast<DWORD>(std::min<std::size_t>(bytes,4u*1024u*1024u)),written))return false;
            ptr+=written;bytes-=written;
        }
        return true;
    }
    void run() {
        Bytes isolate;
        std::vector<std::uint16_t> yuv;
        try {
            for(;;){
                Frame frame;
                {
                    std::unique_lock lock(mutex_);
                    ready_.wait(lock,[&]{return ending_||!queue_.empty();});
                    if(queue_.empty())break;
                    queuedBytes_-=queue_.front().bytes();
                    frame=std::move(queue_.front());queue_.pop_front();
                    ready_.notify_all();
                }
                bool written{};
                if(!frame.depth.empty()){
                    if(frame.depthRange)
                        for(auto& sample:frame.depth)sample=frame.depthRange->normalized(sample);
                    depth_export::pack(frame.depth,yuv,depth_.dither,depth_.strength,depth_.fullRange);
                    written=write(yuv.data(),yuv.size()*sizeof(yuv[0]));
                }else if(frame.coverage){
                    const auto& source=*frame.pixels;const auto& mask=*frame.coverage;
                    isolate.resize(source.size());
                    for(std::size_t i=0;i<source.size();i+=4){
                        const auto alpha=mask[i+frame.channel];
                        isolate[i]=alpha?source[i]:0;isolate[i+1]=alpha?source[i+1]:0;
                        isolate[i+2]=alpha?source[i+2]:0;isolate[i+3]=alpha;
                    }
                    written=write(isolate.data(),isolate.size());
                }else if(frame.pixels)written=write(frame.pixels->data(),frame.pixels->size());
                if(!written){
                    fail(input_.error()==ERROR_TIMEOUT?"encoder stopped reading frames":"encoder rejected a frame");
                    if(input_.error()==ERROR_TIMEOUT)TerminateProcess(process_,1);
                    break;
                }
            }
        }catch(const std::exception& e){fail(e.what());}
        input_.close();
        if(WaitForSingleObject(process_,30000)!=WAIT_OBJECT_0){
            TerminateProcess(process_,1);fail("encoder finalization timed out");
            WaitForSingleObject(process_,2000);
        }
        DWORD code{};
        if(!GetExitCodeProcess(process_,&code)||code!=0)fail("encoder failed");
        CloseHandle(process_);
    }

    BoundedPipeWriter input_;
    HANDLE process_{};
    std::string name_,error_;
    DepthOptions depth_;
    std::thread worker_;
    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<Frame> queue_;
    std::size_t queuedBytes_{};
    bool ending_{};
};
}
