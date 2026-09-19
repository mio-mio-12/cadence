#pragma once
#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif
namespace take::detail {
// Same-volume staging and atomic publication; never delete the original on failure.
template<class Writer>
bool atomicFile(const std::filesystem::path& destination,std::string& error,Writer writer){
    static std::atomic<unsigned long long> serial{};
    std::filesystem::path directory;
    for(unsigned attempt=0;attempt<32;++attempt){
        auto candidate=destination;
        candidate+=std::filesystem::path(".saving-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(serial++));
        std::error_code ec;
        if(std::filesystem::create_directory(candidate,ec)){directory=std::move(candidate);break;}
        if(ec&&ec!=std::errc::file_exists){error="Could not stage take: "+ec.message();return false;}
    }
    if(directory.empty()){error="Could not reserve a temporary take file";return false;}
    const auto temporary=directory/"payload";
    struct Cleanup {
        std::filesystem::path file,directory;
        ~Cleanup(){std::error_code ec;std::filesystem::remove(file,ec);std::filesystem::remove(directory,ec);}
    } cleanup{temporary,directory};
    if(!writer(temporary)){if(error.empty())error="Could not write complete take; original preserved";return false;}
#ifdef _WIN32
    if(!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        error="Could not replace take; original preserved (Windows error "+std::to_string(GetLastError())+")";return false;
    }
#else
    std::error_code ec;std::filesystem::rename(temporary,destination,ec);
    if(ec){error="Could not replace take; original preserved: "+ec.message();return false;}
#endif
    return true;
}
}
